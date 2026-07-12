#include "gptapi_responses.h"
#include "cppJSON.h"
#include <curl/curl.h>
#include <string>
#include <cstring>
#include <unistd.h>

using namespace std;

#define LOG(a,...) fprintf(stderr, "<gptapi_responses:%d>%s: " a "\n", __LINE__, __func__, ##__VA_ARGS__)

static cppJSON responses_content(cJSON *content, const char *text_type) {
    cppJSON result("[]");
    if (!content) return result;

    if (cJSON_IsString(content)) {
        cppJSON part("{}");
        part.insert("type", text_type);
        part.insert("text", content->valuestring ? content->valuestring : "");
        result.push_back(std::move(part));
        return result;
    }

    if (!cJSON_IsArray(content)) {
        result.push_back(cppJSON(content, false).clone());
        return result;
    }

    for (cJSON *item = content->child; item; item = item->next) {
        if (!cJSON_IsObject(item)) continue;
        cJSON *type = cJSON_GetObjectItem(item, "type");
        if (!cJSON_IsString(type)) continue;

        if (strcmp(type->valuestring, "text") == 0) {
            cJSON *text = cJSON_GetObjectItem(item, "text");
            cppJSON part("{}");
            part.insert("type", text_type);
            part.insert("text", cJSON_IsString(text) && text->valuestring ? text->valuestring : "");
            result.push_back(std::move(part));
        } else if (strcmp(type->valuestring, "image_url") == 0) {
            cJSON *image_url = cJSON_GetObjectItem(item, "image_url");
            cJSON *url = cJSON_IsObject(image_url) ? cJSON_GetObjectItem(image_url, "url") : nullptr;
            if (!cJSON_IsString(url) || !url->valuestring) continue;

            cppJSON part("{}");
            part.insert("type", "input_image");
            part.insert("image_url", url->valuestring);
            if (cJSON_IsObject(image_url)) {
                cJSON *detail = cJSON_GetObjectItem(image_url, "detail");
                if (cJSON_IsString(detail) && detail->valuestring)
                    part.insert("detail", detail->valuestring);
            }
            result.push_back(std::move(part));
        } else {
            result.push_back(cppJSON(item, false).clone());
        }
    }
    return result;
}

static cppJSON make_responses_request(const cppJSON& chat_request, bool stream) {
    cppJSON request = chat_request.clone();
    cppJSON messages = request["messages"];
    cppJSON input("[]");

    if (messages && cJSON_IsArray(messages.a)) {
        for (cJSON *message = messages.a->child; message; message = message->next) {
            if (!cJSON_IsObject(message)) continue;
            cJSON *role = cJSON_GetObjectItem(message, "role");
            cJSON *content = cJSON_GetObjectItem(message, "content");
            cppJSON item("{}");
            const char *text_type = "input_text";
            if (cJSON_IsString(role) && role->valuestring)
                item.insert("role", role->valuestring);
            if (cJSON_IsString(role) && strcmp(role->valuestring, "assistant") == 0)
                text_type = "output_text";
            item.insert("content", responses_content(content, text_type));
            cJSON *name = cJSON_GetObjectItem(message, "name");
            if (cJSON_IsString(name) && name->valuestring)
                item.insert("name", name->valuestring);
            input.push_back(std::move(item));
        }
    }

    request.erase("messages");
    request.erase("ida");
    request.erase("stream_options");
    request.erase("enable_thinking");
    if (request.has("max_tokens") && !request.has("max_output_tokens")) {
        cppJSON max_tokens = request["max_tokens"].clone();
        request.erase("max_tokens");
        request.insert("max_output_tokens", std::move(max_tokens));
    }
    request.insert("input", std::move(input));
    request.insert("stream", stream);
    return request;
}

static void send_frontend_delta(http_para *a, const char *field, const string& value) {
    if (value.empty()) return;
    cppJSON delta("{}");
    delta.insert(field, value);
    cppJSON choice("{}");
    choice.insert("index", 0.0);
    choice.insert("delta", std::move(delta));
    cppJSON choices("[]");
    choices.push_back(std::move(choice));
    cppJSON event("{}");
    event.insert("choices", std::move(choices));
    string line = "data: " + event.stringify_Unformatted() + "\r\n\r\n";
    if (write(a->cl, line.data(), line.size()));
}

struct response_stream_data {
    http_para *a;
    string pending;
    string assistant_reply;
    string event_name;
    string raw_response;
    bool sent_header = false;
    bool sent_done = false;
    bool failed = false;
};

static void ensure_frontend_header(response_stream_data *data) {
    if (data->sent_header) return;
    data->a->m = 1;
    const char *header = Hok "Content-Type: text/event-stream\r\n" Hc0 "\r\n";
    if (write(data->a->cl, header, strlen(header)));
    data->sent_header = true;
}

static void handle_response_data(response_stream_data *data, const string& json_text) {
    if (json_text == "[DONE]") {
        if (!data->sent_done) {
            const char *done = "data: [DONE]\r\n\r\n";
            if (write(data->a->cl, done, strlen(done)));
            data->sent_done = true;
        }
        return;
    }

    cppJSON event(json_text.c_str());
    if (!event) {
        LOG("invalid SSE data length=%zu", json_text.length());
        return;
    }
    if (event.has("error")) {
        cppJSON error = event["error"];
        string message = error["message"].valuestring();
        if (message.empty()) message = "Responses API request failed";
        LOG("upstream error: %s", message.c_str());
        data->failed = true;
        send_frontend_delta(data->a, "content", message);
        return;
    }
    string type = event.has("type") ? event["type"].valuestring() : data->event_name;
    if (type == "response.output_text.delta") {
        string text = event["delta"].valuestring();
        data->assistant_reply += text;
        send_frontend_delta(data->a, "content", text);
    } else if (type == "response.reasoning_summary_text.delta" ||
               type == "response.reasoning_text.delta") {
        string text = event["delta"].valuestring();
        send_frontend_delta(data->a, "reasoning_content", text);
    } else if (type == "response.completed") {
        cppJSON response = event["response"];
        cppJSON usage = response["usage"];
        if (usage) {
            cppJSON out_usage("{}");
            out_usage.insert("completion_tokens", usage["output_tokens"].valuedouble());
            out_usage.insert("prompt_tokens", usage["input_tokens"].valuedouble());
            out_usage.insert("total_tokens", usage["total_tokens"].valuedouble());
            cppJSON usage_event("{}");
            usage_event.insert("usage", std::move(out_usage));
            string line = "data: " + usage_event.stringify_Unformatted() + "\r\n\r\n";
            if (write(data->a->cl, line.data(), line.size()));
        }
        if (!data->sent_done) {
            const char *done = "data: [DONE]\r\n\r\n";
            if (write(data->a->cl, done, strlen(done)));
            data->sent_done = true;
        }
    } else if (type == "error" || type == "response.failed") {
        string message = event["message"].valuestring();
        if (message.empty()) message = event["response"]["error"]["message"].valuestring();
        if (message.empty()) message = "Responses API request failed";
        data->failed = true;
        send_frontend_delta(data->a, "content", message);
    }
}

static size_t response_stream_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    response_stream_data *data = (response_stream_data *)userdata;
    size_t total = size * nmemb;
    ensure_frontend_header(data);

    data->pending.append((char *)ptr, total);
    data->raw_response.append((char *)ptr, total);
    size_t end;
    while ((end = data->pending.find('\n')) != string::npos) {
        string line = data->pending.substr(0, end);
        data->pending.erase(0, end + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("event:", 0) == 0) {
            data->event_name = line.substr(6);
            while (!data->event_name.empty() && data->event_name[0] == ' ')
                data->event_name.erase(0, 1);
        } else if (line.rfind("data:", 0) == 0) {
            string payload = line.substr(5);
            while (!payload.empty() && payload[0] == ' ') payload.erase(0, 1);
            handle_response_data(data, payload);
            data->event_name.clear();
        }
    }
    return total;
}

static size_t string_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    string *response = (string *)userdata;
    response->append((char *)ptr, size * nmemb);
    return size * nmemb;
}

static struct curl_slist *make_headers(const string& auth_token) {
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    string auth = "Authorization: " + auth_token;
    headers = curl_slist_append(headers, auth.c_str());
    return headers;
}

static string response_output_text(const cppJSON& response);

static void handle_non_sse_response(response_stream_data *data) {
    if (data->raw_response.empty()) return;
    cppJSON response(data->raw_response.c_str());
    if (response) {
        if (response.has("error")) {
            cppJSON error = response["error"];
            string message = error["message"].valuestring();
            if (message.empty()) message = "Responses API request failed";
            data->failed = true;
            ensure_frontend_header(data);
            send_frontend_delta(data->a, "content", message);
            return;
        }
        string text = response_output_text(response);
        if (!text.empty()) {
            data->assistant_reply = text;
            ensure_frontend_header(data);
            send_frontend_delta(data->a, "content", text);
            return;
        }
    }
    if (data->raw_response.rfind("event:", 0) != 0 && data->raw_response.rfind("data:", 0) != 0) {
        data->failed = true;
        ensure_frontend_header(data);
        send_frontend_delta(data->a, "content", data->raw_response);
    }
}

bool gpt_responses_request(http_para *a, const string& api_url,
                           const string& auth_token, const cppJSON& chat_request,
                           string& assistant_reply) {
    cppJSON request = make_responses_request(chat_request, true);
    string post_data = request.stringify_Unformatted();
    LOG("request url=%s", api_url.c_str());
    response_stream_data data{a};
    CURL *curl = curl_easy_init();
    if (!curl) return false;
    struct curl_slist *headers = make_headers(auth_token);
    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_stream_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1200L);
    CURLcode result = curl_easy_perform(curl);
    long http_status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
    if (!data.pending.empty()) {
        LOG("processing trailing SSE data length=%zu", data.pending.length());
        string line = data.pending;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("data:", 0) == 0) {
            string payload = line.substr(5);
            while (!payload.empty() && payload[0] == ' ') payload.erase(0, 1);
            handle_response_data(&data, payload);
        } else {
            LOG("trailing response length=%zu", line.length());
        }
    }
    if (data.assistant_reply.empty() && !data.failed) handle_non_sse_response(&data);
    if (result != CURLE_OK && data.assistant_reply.empty()) {
        data.failed = true;
        ensure_frontend_header(&data);
        send_frontend_delta(a, "content", curl_easy_strerror(result));
    } else if (http_status >= 400 && data.assistant_reply.empty() && !data.failed) {
        data.failed = true;
        ensure_frontend_header(&data);
        send_frontend_delta(a, "content", "上游请求失败，HTTP " + to_string(http_status));
    }
    LOG("curl result=%d assistant_reply_length=%zu", (int)result, data.assistant_reply.length());
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (!data.sent_done) {
        const char *done = "data: [DONE]\r\n\r\n";
        if (write(a->cl, done, strlen(done)));
    }
    assistant_reply = data.assistant_reply;
    return result == CURLE_OK && !data.failed;
}

static string response_output_text(const cppJSON& response) {
    if (response.has("output_text")) return response["output_text"].valuestring();
    cppJSON output = response["output"];
    if (!output || !cJSON_IsArray(output.a)) return "";
    string text;
    for (cppJSON item = output.child(); item; item = item.next()) {
        cppJSON content = item["content"];
        if (!content || !cJSON_IsArray(content.a)) continue;
        for (cppJSON part = content.child(); part; part = part.next())
            if (part.has("text")) text += part["text"].valuestring();
    }
    return text;
}

bool gpt_responses_title(const string& api_url, const string& auth_token,
                         const cppJSON& chat_request, string& title) {
    cppJSON request = make_responses_request(chat_request, false);
    string post_data = request.stringify_Unformatted();
    string response_text;
    CURL *curl = curl_easy_init();
    if (!curl) return false;
    struct curl_slist *headers = make_headers(auth_token);
    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_text);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    CURLcode result = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK) return false;
    cppJSON response(response_text.c_str());
    if (!response) return false;
    title = response_output_text(response);
    return !title.empty();
}

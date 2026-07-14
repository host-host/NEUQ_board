#include "gptapi4.h"
#include "cppJSON.h"
#include <curl/curl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <strings.h>

using namespace std;

struct upstream_response_meta {
    string content_type;
};

static size_t upstream_header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    upstream_response_meta* meta = (upstream_response_meta*)userdata;
    size_t total = size * nitems;
    string line(buffer, total);
    size_t colon = line.find(':');
    if (colon != string::npos) {
        string name = line.substr(0, colon);
        for (char& c : name) c = (char)tolower((unsigned char)c);
        if (name == "content-type") {
            string value = line.substr(colon + 1);
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) value.pop_back();
            meta->content_type = value;
        }
    }
    return total;
}

static bool upstream_is_stream(const upstream_response_meta& meta) {
    string content_type = meta.content_type;
    for (char& c : content_type) c = (char)tolower((unsigned char)c);
    return content_type.find("text/event-stream") != string::npos;
}

static void send_stream_header(http_para* a, const string& id) {
    if (!a) return;
    string header = Hok "Content-Type: text/event-stream\r\n" Hc0
                    "X-Conversation-ID: " + id + "\r\n\r\n";
    if (write(a->cl, header.data(), header.size()));
}

static void send_raw_upstream_response(http_para* a, const upstream_response_meta& meta,
                                       const string& id, const string& body) {
    if (!a) return;
    string header = Hok;
    if (!meta.content_type.empty()) header += "Content-Type: " + meta.content_type + "\r\n";
    header += "X-Conversation-ID: " + id + "\r\n" Hc0 "\r\n";
    if (write(a->cl, header.data(), header.size()));
    if (write(a->cl, body.data(), body.size()));
}

static bool gpt2_is_string(const cppJSON& value) {
    return value && cJSON_IsString(value.a) && value.a->valuestring && value.a->valuestring[0];
}

static bool gpt2_is_string_array(const cppJSON& value) {
    if (!value || !cJSON_IsArray(value.a)) return false;
    for (cppJSON item = value.child(); item; item = item.next())
        if (!cJSON_IsString(item.a)) return false;
    return true;
}

static bool gpt2_config_valid(const cppJSON& config) {
    if (!config || !cJSON_IsArray(config.a)) return false;
    for (cppJSON provider = config.child(); provider; provider = provider.next()) {
        if (!cJSON_IsObject(provider.a)) return false;
        if (!gpt2_is_string(provider["provider"])) return false;
        if (!gpt2_is_string(provider["url"])) return false;
        if (!gpt2_is_string_array(provider["Authorization"])) return false;
        if (provider.has("parallel") && !cJSON_IsNumber(provider["parallel"].a)) return false;
        if (provider.has("public") && !gpt2_is_string_array(provider["public"])) return false;
        if (provider.has("private") && !gpt2_is_string_array(provider["private"])) return false;
        if (provider.has("unavailable") && !gpt2_is_string_array(provider["unavailable"])) return false;
        if (provider.has("format") && !gpt2_is_string(provider["format"])) return false;
    }
    return true;
}

char* getgpt2json() {
    FILE* file = fopen("/web/res/pri/gpt2.json", "rb");
    if (!file) return nullptr;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return nullptr;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return nullptr;
    }
    char* buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return nullptr;
    }
    size_t read_size = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (read_size != (size_t)size) {
        free(buffer);
        return nullptr;
    }
    buffer[size] = '\0';
    cppJSON config(buffer);
    if (!gpt2_config_valid(config)) {
        free(buffer);
        return nullptr;
    }
    return buffer;
}

static cppJSON responses_content(cJSON* content, const char* text_type) {
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
    for (cJSON* item = content->child; item; item = item->next) {
        if (!cJSON_IsObject(item)) continue;
        cJSON* type = cJSON_GetObjectItem(item, "type");
        if (!cJSON_IsString(type)) continue;
        if (strcmp(type->valuestring, "text") == 0) {
            cJSON* text = cJSON_GetObjectItem(item, "text");
            cppJSON part("{}");
            part.insert("type", text_type);
            part.insert("text", cJSON_IsString(text) ? text->valuestring : "");
            result.push_back(std::move(part));
        } else if (strcmp(type->valuestring, "image_url") == 0) {
            cJSON* image_url = cJSON_GetObjectItem(item, "image_url");
            cJSON* url = cJSON_IsObject(image_url) ? cJSON_GetObjectItem(image_url, "url") : nullptr;
            if (!cJSON_IsString(url)) continue;
            cppJSON part("{}");
            part.insert("type", "input_image");
            part.insert("image_url", url->valuestring);
            result.push_back(std::move(part));
        } else {
            result.push_back(cppJSON(item, false).clone());
        }
    }
    return result;
}

static cppJSON make_responses_request(const cppJSON& chat_request) {
    cppJSON request = chat_request.clone();
    cppJSON messages = request["messages"];
    cppJSON input("[]");
    if (messages && cJSON_IsArray(messages.a)) {
        for (cJSON* message = messages.a->child; message; message = message->next) {
            if (!cJSON_IsObject(message)) continue;
            cJSON* role = cJSON_GetObjectItem(message, "role");
            cJSON* content = cJSON_GetObjectItem(message, "content");
            cppJSON item("{}");
            const char* text_type = "input_text";
            if (cJSON_IsString(role)) item.insert("role", role->valuestring);
            if (cJSON_IsString(role) && strcmp(role->valuestring, "assistant") == 0)
                text_type = "output_text";
            item.insert("content", responses_content(content, text_type));
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
    request.insert("stream", true);
    return request;
}

struct responses_stream_data {
    http_para* a;
    string id;
    upstream_response_meta meta;
    string pending;
    string event_name;
    string raw_response;
    string assistant_reply;
    bool failed = false;
    bool sent_done = false;
    bool sent_header = false;
};

static void responses_send_delta(http_para* a, const char* field, const string& value) {
    if (!a || value.empty()) return;
    cppJSON delta("{}");
    delta.insert(field, value.c_str());
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

static void responses_send_done(responses_stream_data* data) {
    if (data->sent_done) return;
    if (!data->a) {
        data->sent_done = true;
        return;
    }
    const char* done = "data: [DONE]\r\n\r\n";
    if (write(data->a->cl, done, strlen(done)));
    data->sent_done = true;
}

static void responses_handle_event(responses_stream_data* data, const string& text) {
    if (text == "[DONE]") {
        responses_send_done(data);
        return;
    }
    cppJSON event(text.c_str());
    if (!event) return;
    string type = event.has("type") ? event["type"].valuestring() : data->event_name;
    if (type == "response.output_text.delta") {
        string value = event["delta"].valuestring();
        data->assistant_reply += value;
        responses_send_delta(data->a, "content", value);
    } else if (type == "response.reasoning_summary_text.delta" ||
               type == "response.reasoning_text.delta") {
        responses_send_delta(data->a, "reasoning_content", event["delta"].valuestring());
    } else if (type == "response.completed") {
        cppJSON usage = event["response"]["usage"];
        if (usage) {
            cppJSON out_usage("{}");
            out_usage.insert("completion_tokens", usage["output_tokens"].valuedouble());
            out_usage.insert("prompt_tokens", usage["input_tokens"].valuedouble());
            out_usage.insert("total_tokens", usage["total_tokens"].valuedouble());

            // Responses API 的 usage 事件本身只有 usage 字段；统一包装成
            // Completions 的最后一个 chunk，保证现有客户端可以按
            // choices[0].delta 和 usage 解析。
            cppJSON delta("{}");
            delta.insert("content", "");
            delta.insert("reasoning_content", (const char*)nullptr);
            cppJSON choice("{}");
            choice.insert("index", 0.0);
            choice.insert("delta", std::move(delta));
            choice.insert("logprobs", (const char*)nullptr);
            choice.insert("finish_reason", "stop");
            cppJSON choices("[]");
            choices.push_back(std::move(choice));

            cppJSON usage_event("{}");
            usage_event.insert("object", "chat.completion.chunk");
            usage_event.insert("choices", std::move(choices));
            usage_event.insert("usage", std::move(out_usage));
            string line = "data: " + usage_event.stringify_Unformatted() + "\r\n\r\n";
            if (data->a && write(data->a->cl, line.data(), line.size()));
        }
        responses_send_done(data);
    } else if (type == "error" || type == "response.failed" || event.has("error")) {
        string message = event["message"].valuestring();
        if (message.empty()) message = event["error"]["message"].valuestring();
        if (message.empty()) message = "Responses API request failed";
        data->failed = true;
        responses_send_delta(data->a, "content", message);
    }
}

static size_t responses_write_callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    responses_stream_data* data = (responses_stream_data*)userdata;
    size_t total = size * nmemb;
    data->pending.append((char*)ptr, total);
    data->raw_response.append((char*)ptr, total);
    if (!upstream_is_stream(data->meta)) return total;
    if (!data->sent_header) {
        send_stream_header(data->a, data->id);
        data->sent_header = true;
    }
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
            responses_handle_event(data, payload);
            data->event_name.clear();
        }
    }
    return total;
}

static string responses_output_text(const cppJSON& response) {
    if (response.has("output_text")) return response["output_text"].valuestring();
    string text;
    cppJSON output = response["output"];
    if (!output || !cJSON_IsArray(output.a)) return text;
    for (cppJSON item = output.child(); item; item = item.next()) {
        cppJSON content = item["content"];
        if (!content || !cJSON_IsArray(content.a)) continue;
        for (cppJSON part = content.child(); part; part = part.next())
            if (part.has("text")) text += part["text"].valuestring();
    }
    return text;
}

struct completions_write_data {
    http_para* a;
    string id;
    upstream_response_meta meta;
    string response;
    bool sent_header = false;
};

static size_t completions_write_callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    completions_write_data* data = (completions_write_data*)userdata;
    size_t total = size * nmemb;
    data->response.append((char*)ptr, total);
    if (upstream_is_stream(data->meta)) {
        if (!data->sent_header) {
            send_stream_header(data->a, data->id);
            data->sent_header = true;
        }
        if (data->a && write(data->a->cl, ptr, total));
    }
    return total;
}

static cppJSON extract_completions_reply(const string& response) {
    string content;
    size_t pos = 0;
    while (pos < response.size()) {
        size_t end = response.find('\n', pos);
        string line = end == string::npos ? response.substr(pos) : response.substr(pos, end - pos);
        pos = end == string::npos ? response.size() : end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("data: ", 0) != 0) continue;
        string json_text = line.substr(6);
        if (json_text == "[DONE]") break;
        cppJSON event(json_text.c_str());
        if (!event) continue;
        cppJSON choices = event["choices"];
        if (!choices || !cJSON_IsArray(choices.a) || cJSON_GetArraySize(choices.a) == 0) continue;
        cppJSON choice = choices[0];
        cppJSON delta = choice["delta"];
        if (delta && delta.has("content") && cJSON_IsString(delta["content"].a))
            content += delta["content"].valuestring();
        else if (choice["message"].has("content") && cJSON_IsString(choice["message"]["content"].a))
            content += choice["message"]["content"].valuestring();
    }
    if (content.empty()) {
        cppJSON normal(response.c_str());
        cppJSON choices = normal["choices"];
        if (choices && cJSON_IsArray(choices.a) && cJSON_GetArraySize(choices.a) > 0) {
            cppJSON choice = choices[0];
            if (choice["message"].has("content") && cJSON_IsString(choice["message"]["content"].a))
                content = choice["message"]["content"].valuestring();
            else if (choice.has("text") && cJSON_IsString(choice["text"].a))
                content = choice["text"].valuestring();
        }
    }
    cppJSON reply("{}");
    reply.insert("role", "assistant");
    reply.insert("content", content.c_str());
    return reply;
}

cppJSON gpt_completions(http_para* a, const std::string& id,const string& url, const string& auth, const string& content) {
    completions_write_data data;
    data.a = a;
    data.id = id;
    CURL* curl = curl_easy_init();
    if (!curl) return cppJSON();

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    string auth_header = "Authorization: " + auth;
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, upstream_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &data.meta);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, completions_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1200L);
    CURLcode result = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK) return cppJSON();
    if (!upstream_is_stream(data.meta)) {
        send_raw_upstream_response(a, data.meta, id, data.response);
    }
    if (upstream_is_stream(data.meta) && !data.sent_header) send_stream_header(a, id);
    return extract_completions_reply(data.response);
}

cppJSON gpt_responses(http_para* a, const std::string& id,const string& url, const string& auth, const string& content) {
    cppJSON chat_request(content.c_str());
    if (!chat_request) return cppJSON();
    cppJSON request = make_responses_request(chat_request);
    string post_data = request.stringify_Unformatted();
    responses_stream_data data;
    data.a = a;
    data.id = id;
    CURL* curl = curl_easy_init();
    if (!curl) return cppJSON();
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    string auth_header = "Authorization: " + auth;
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, upstream_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &data.meta);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, responses_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1200L);
    CURLcode result = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (!upstream_is_stream(data.meta)) {
        send_raw_upstream_response(a, data.meta, id, data.raw_response);
        cppJSON response(data.raw_response.c_str());
        if (response) {
            cppJSON reply("{}");
            reply.insert("role", "assistant");
            reply.insert("content", responses_output_text(response).c_str());
            return reply;
        }
        return cppJSON();
    }

    if (!data.pending.empty()) {
        string line = data.pending;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("data:", 0) == 0) {
            string payload = line.substr(5);
            while (!payload.empty() && payload[0] == ' ') payload.erase(0, 1);
            responses_handle_event(&data, payload);
        }
    }
    if (data.assistant_reply.empty() && !data.failed && !data.raw_response.empty()) {
        cppJSON response(data.raw_response.c_str());
        if (response) {
            data.assistant_reply = responses_output_text(response);
            if (!data.assistant_reply.empty())
                responses_send_delta(a, "content", data.assistant_reply);
        }
    }
    if (result != CURLE_OK) {
        data.failed = true;
        responses_send_delta(a, "content", curl_easy_strerror(result));
    }
    responses_send_done(&data);
    if (data.failed || result != CURLE_OK) return cppJSON();
    cppJSON reply("{}");
    reply.insert("role", "assistant");
    reply.insert("content", data.assistant_reply.c_str());
    return reply;
}

cppJSON gpt_req(http_para* a,const std::string& id,const std::string& url,const std::string& auth,const std::string& content,const std::string& format){
    if(format.empty()||format=="completions")return gpt_completions(a,id,url,auth,content);
    if(format=="responses")return gpt_responses(a,id,url,auth,content);
    return cppJSON();
}

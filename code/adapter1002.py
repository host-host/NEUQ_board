import json
import time
from fastapi import FastAPI, Request, Response
from fastapi.responses import StreamingResponse
from fastapi.middleware.cors import CORSMiddleware
import httpx
import uvicorn

# ================= 调试配置 =================
DEBUG = 1  # 设为 1 时打印请求和上游返回的完整响应调试信息，设为 0 关闭
# ============================================

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

TARGET_URL = "https://right.codes/claude-aws/v1/messages"


def convert_openai_to_claude(openai_req: dict) -> dict:
    claude_req = {}
    openai_model = openai_req.get("model")
    claude_req["model"] = openai_model
    claude_req["stream"] = openai_req.get("stream", False)
    claude_req["max_tokens"] = openai_req.get("max_tokens", 4096)
    claude_req["tool_choice"] = {"type": "none"}
    if "temperature" in openai_req:
        claude_req["temperature"] = openai_req["temperature"]

    claude_messages = []
    for msg in openai_req.get("messages", []):
        role = msg.get("role")
        content = msg.get("content", "")
        
        if role == "system":
            pass  # 忽略 system 角色消息
        else:
            claude_content = []
            # 判断 content 是否为多模态数组格式
            if isinstance(content, list):
                for item in content:
                    item_type = item.get("type")
                    if item_type == "text":
                        claude_content.append({
                            "type": "text",
                            "text": item.get("text", "")
                        })
                    elif item_type == "image_url":
                        img_url = item.get("image_url", {}).get("url", "")
                        # 仅处理前端传输过来的 base64 data url
                        if img_url.startswith("data:"):
                            try:
                                # 示例格式: data:image/jpeg;base64,/9j/4AAQSkZJRg...
                                header, base64_data = img_url.split(";base64,")
                                media_type = header.split("data:")[1]  # 获取 image/jpeg, image/png 等
                                claude_content.append({
                                    "type": "image",
                                    "source": {
                                        "type": "base64",
                                        "media_type": media_type,
                                        "data": base64_data
                                    }
                                })
                            except Exception as e:
                                if DEBUG == 1:
                                    print(f"[DEBUG ERROR] 转换 Base64 图片失败: {str(e)}")
            else:
                # 兼容单字符串格式
                claude_content.append({
                    "type": "text",
                    "text": str(content)
                })

            claude_messages.append({
                "role": role,
                "content": claude_content
            })
            
    claude_req["messages"] = claude_messages
    return claude_req

def convert_claude_to_openai_json(claude_resp: dict, created_time: int) -> dict:
    text = ""
    for block in claude_resp.get("content", []):
        if block.get("type") == "text":
            text += block.get("text", "")
    stop_reason = claude_resp.get("stop_reason")
    finish_reason = "stop" if stop_reason == "end_turn" else (stop_reason or "stop")
    usage = claude_resp.get("usage", {})
    prompt_tokens = usage.get("input_tokens", 0)
    completion_tokens = usage.get("output_tokens", 0)
    return {
        "id": claude_resp.get("id", f"chatcmpl-{int(time.time())}"),
        "object": "chat.completion",
        "created": created_time,
        "model": claude_resp.get("model", "claude-model"),
        "choices": [
            {
                "index": 0,
                "message": {
                    "role": "assistant",
                    "content": text
                },
                "finish_reason": finish_reason
            }
        ],
        "usage": {
            "prompt_tokens": prompt_tokens,
            "completion_tokens": completion_tokens,
            "total_tokens": prompt_tokens + completion_tokens
        }
    }

@app.post("/v1/chat/completions")
@app.post("/claude-aws/v1/messages")
@app.post("/v1/messages")
async def chat_completions(request: Request):
    body_bytes = await request.body()
    try:
        openai_req = json.loads(body_bytes.decode("utf-8"))
    except Exception:
        return {"error": "Invalid JSON body"}

    claude_req = convert_openai_to_claude(openai_req)
    
    if DEBUG == 1:
        # 截断打印，防止图片 base64 过长刷屏
        debug_req_copy = json.loads(json.dumps(claude_req))
        for msg in debug_req_copy.get("messages", []):
            if isinstance(msg.get("content"), list):
                for block in msg["content"]:
                    if block.get("type") == "image":
                        # 仅展示前 50 个字符用于调试
                        block["source"]["data"] = block["source"]["data"][:50] + "...[BASE64 TRUNCATED]..."
        print("\n=== [DEBUG] 转换后的 Claude 请求载荷 ===")
        print(json.dumps(debug_req_copy, ensure_ascii=False, indent=2))
        print("=========================================\n")

    headers = {
        "Content-Type": "application/json",
        "anthropic-version": "2023-06-01"
    }
    auth_header = request.headers.get("Authorization")
    if auth_header:
        headers["Authorization"] = auth_header

    is_stream = openai_req.get("stream", False)
    created_time = int(time.time())
    client = httpx.AsyncClient(timeout=60.0)

    if is_stream:
        async def stream_generator():
            try:
                msg_id = f"chatcmpl-{int(time.time())}"
                model_name = claude_req["model"]
                current_event = None

                async with client.stream("POST", TARGET_URL, json=claude_req, headers=headers) as r:
                    if DEBUG == 1:
                        print(f"=== [DEBUG] 接收流式响应。状态码: {r.status_code} ===")
                        print(f"=== [DEBUG] 响应头: {dict(r.headers)} ===")
                        print("=== [DEBUG] 流数据开始推送 ===")

                    if r.status_code != 200:
                        err_content = await r.aread()
                        if DEBUG == 1:
                            print(f"[DEBUG ERROR] 上游流请求失败: {err_content.decode()}")
                        yield f"data: {json.dumps({'error': err_content.decode()})}\n\n"
                        return

                    async for line in r.aiter_lines():
                        if DEBUG == 1:
                            print(f"[DEBUG LINE] {line}")
                        
                        line = line.strip()
                        if not line:
                            continue

                        if line.startswith("event:"):
                            current_event = line[len("event:"):].strip()
                        elif line.startswith("data:"):
                            data_str = line[len("data:"):].strip()
                            try:
                                data = json.loads(data_str)
                            except json.JSONDecodeError:
                                continue

                            if current_event == "message_start":
                                msg = data.get("message", {})
                                if "id" in msg:
                                    msg_id = msg["id"]
                                if "model" in msg:
                                    model_name = msg["model"]
                            elif current_event == "content_block_delta":
                                text = data.get("delta", {}).get("text", "")
                                openai_chunk = {
                                    "id": msg_id,
                                    "object": "chat.completion.chunk",
                                    "created": created_time,
                                    "model": model_name,
                                    "choices": [{
                                        "index": 0,
                                        "delta": {"content": text},
                                        "finish_reason": None
                                    }],
                                    "usage": None
                                }
                                yield f"data: {json.dumps(openai_chunk, ensure_ascii=False)}\n\n"
                            elif current_event == "message_delta":
                                delta = data.get("delta", {})
                                stop_reason = delta.get("stop_reason")
                                finish_reason = "stop" if stop_reason == "end_turn" else (stop_reason or "stop")
                                usage = data.get("usage", {})
                                prompt_tokens = usage.get("input_tokens", 0)
                                completion_tokens = usage.get("output_tokens", 0)
                                openai_chunk = {
                                    "id": msg_id,
                                    "object": "chat.completion.chunk",
                                    "created": created_time,
                                    "model": model_name,
                                    "choices": [{
                                        "index": 0,
                                        "delta": {},
                                        "finish_reason": finish_reason
                                    }]
                                }
                                if prompt_tokens or completion_tokens:
                                    openai_chunk["usage"] = {
                                        "prompt_tokens": prompt_tokens,
                                        "completion_tokens": completion_tokens,
                                        "total_tokens": prompt_tokens + completion_tokens
                                    }
                                yield f"data: {json.dumps(openai_chunk, ensure_ascii=False)}\n\n"
                            elif current_event == "message_stop":
                                yield "data: [DONE]\n\n"
                                if DEBUG == 1:
                                    print("=== [DEBUG] 流数据结束推送 ===\n")
            except Exception as e:
                if DEBUG == 1:
                    print(f"[DEBUG ERROR] 发生异常: {str(e)}")
                yield f"data: {json.dumps({'error': str(e)})}\n\n"
            finally:
                await client.aclose()

        return StreamingResponse(stream_generator(), media_type="text/event-stream")
    else:
        try:
            r = await client.post(TARGET_URL, json=claude_req, headers=headers)
            await client.aclose()
            
            if DEBUG == 1:
                print(f"=== [DEBUG] 接收非流式响应。状态码: {r.status_code} ===")
                print(f"=== [DEBUG] 响应原文: ===")
                print(r.text)
                print("=======================================\n")

            if r.status_code != 200:
                return Response(content=r.content, status_code=r.status_code, media_type="application/json")
            claude_resp = r.json()
            openai_resp = convert_claude_to_openai_json(claude_resp, created_time)
            return openai_resp
        except Exception as e:
            await client.aclose()
            if DEBUG == 1:
                print(f"[DEBUG ERROR] 发生异常: {str(e)}")
            return {"error": str(e)}

if __name__ == "__main__":
    uvicorn.run(app, host="127.0.0.1", port=1002)
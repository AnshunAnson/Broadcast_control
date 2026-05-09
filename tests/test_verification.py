"""播控中台 + MCP 全线功能验证脚本。

覆盖:
  - HTTP: /health, /schema/upload, /schema/list, /command/send
  - WebSocket: 连接 / 推送 / set_parameter
  - MCP Tools: execute_stage_cue, get_capabilities, set_float_parameter
  - 端到端: Schema 上报 → 语义匹配 → UDP 打包
"""

import asyncio
import json
import sys
import time

import httpx
import websockets

HTTP_BASE = "http://127.0.0.1:8000"
WS_URL = "ws://127.0.0.1:8000/ws"
PASS = 0
FAIL = 0


def check(desc: str, condition: bool, detail: str = ""):
    global PASS, FAIL
    if condition:
        PASS += 1
        print(f"  ✓ {desc}")
    else:
        FAIL += 1
        print(f"  ✗ {desc}  -- {detail}")


async def test_health():
    print("\n--- [1] HTTP /health ---")
    async with httpx.AsyncClient() as client:
        resp = await client.get(f"{HTTP_BASE}/health")
        check("HTTP 200", resp.status_code == 200, f"got {resp.status_code}")
        data = resp.json()
        check("status=ok", data.get("status") == "ok")
        print(f"     Response: {json.dumps(data, ensure_ascii=False)}")


async def test_schema_upload():
    print("\n--- [2] POST /schema/upload ---")
    schema = {
        "source": "ue5_test",
        "version": 1,
        "parameters": [
            {
                "name": "AR_Hologram_Alpha",
                "type": "float",
                "default": 0.0,
                "min_value": 0.0,
                "max_value": 1.0,
                "tags": ["全息", "透明度", "AR", "淡入淡出"],
                "description": "全息投影透明度",
            },
            {
                "name": "Ambient_Light",
                "type": "float",
                "default": 0.5,
                "min_value": 0.0,
                "max_value": 1.0,
                "tags": ["环境光", "亮度", "灯光"],
                "description": "环境光亮度",
            },
            {
                "name": "Firework_Trigger",
                "type": "event",
                "default": 0.0,
                "tags": ["烟花", "特效", "触发", "转场"],
                "description": "烟花特效触发器",
            },
            {
                "name": "Stage_Fog_Density",
                "type": "float",
                "default": 0.0,
                "min_value": 0.0,
                "max_value": 1.0,
                "tags": ["烟雾", "浓度", "氛围"],
                "description": "舞台烟雾浓度",
            },
        ],
    }
    async with httpx.AsyncClient() as client:
        resp = await client.post(f"{HTTP_BASE}/schema/upload", json=schema)
        check("HTTP 200", resp.status_code == 200, f"got {resp.status_code}")
        data = resp.json()
        check("status=ok", data.get("status") == "ok")
        check("registered=4", data.get("registered") == 4, f"got {data.get('registered')}")
        print(f"     Response: {json.dumps(data, ensure_ascii=False)}")


async def test_schema_list():
    print("\n--- [3] GET /schema/list ---")
    async with httpx.AsyncClient() as client:
        resp = await client.get(f"{HTTP_BASE}/schema/list")
        check("HTTP 200", resp.status_code == 200)
        data = resp.json()
        check("status=ok", data.get("status") == "ok")
        check("parameter_count=4", data.get("parameter_count") == 4)
        names = [p["name"] for p in data.get("parameters", [])]
        print(f"     Parameters: {names}")


async def test_command_send():
    print("\n--- [4] POST /command/send ---")

    async with httpx.AsyncClient() as client:
        # 精确参数名
        resp = await client.post(
            f"{HTTP_BASE}/command/send",
            params={"param_name": "AR_Hologram_Alpha", "value": 0.85},
        )
        check("精确参数 HTTP 200", resp.status_code == 200, f"got {resp.status_code}")
        data = resp.json()
        check("精确参数 success=true", data.get("success") is True)
        print(f"     {json.dumps(data, ensure_ascii=False)}")

        # 语义名
        resp = await client.post(
            f"{HTTP_BASE}/command/send",
            params={"param_name": "调暗环境光", "value": 0.2},
        )
        data = resp.json()
        check("语义参数 success=true", data.get("success") is True, f"got {data}")
        check("语义匹配 Ambient_Light", data.get("param_name") == "Ambient_Light")
        print(f"     {json.dumps(data, ensure_ascii=False)}")

        # 触发事件
        resp = await client.post(
            f"{HTTP_BASE}/command/send",
            params={"param_name": "启动烟花", "value": 1.0},
        )
        data = resp.json()
        check("事件触发 success=true", data.get("success") is True)
        check("事件匹配 Firework_Trigger", data.get("param_name") == "Firework_Trigger")
        print(f"     {json.dumps(data, ensure_ascii=False)}")

        # 不存在的参数 — 用真正的随机 UUID 后缀确保无匹配
        import uuid
        no_match = f"ZXYWQ_NO_MATCH_{uuid.uuid4().hex[:8]}"
        resp = await client.post(
            f"{HTTP_BASE}/command/send",
            params={"param_name": no_match, "value": 0.5},
        )
        data = resp.json()
        check("不存在参数 404", resp.status_code == 404, f"got {resp.status_code}")
        print(f"     {json.dumps(data, ensure_ascii=False)}")


async def test_websocket():
    print("\n--- [5] WebSocket /ws ---")
    async with websockets.connect(WS_URL, max_size=2**20) as ws:
        # 初始 Schema 推送
        msg = json.loads(await ws.recv())
        check("WS schema_current type", msg.get("type") == "schema_current")
        check("WS parameter_count=4", msg.get("parameter_count") == 4)
        print(f"     Received: type={msg['type']} params={msg['parameter_count']}")

        # Ping/Pong
        await ws.send(json.dumps({"type": "ping"}))
        pong = json.loads(await ws.recv())
        check("WS pong", pong.get("type") == "pong")

        # set_parameter 指令
        await ws.send(json.dumps({
            "type": "set_parameter",
            "param_name": "AR_Hologram_Alpha",
            "value": 0.42,
        }))
        result = json.loads(await ws.recv())
        check("WS set_parameter type", result.get("type") == "command_result")
        check("WS set_parameter success", result.get("success") is True)
        print(f"     set_parameter result: {json.dumps(result, ensure_ascii=False)}")


async def test_mcp_tools():
    print("\n--- [6] MCP Tools (直接调用) ---")

    from broadcast_control.mcp_server import (
        _handle_get_capabilities,
        _handle_execute_stage_cue,
        _handle_set_float_parameter,
    )
    from broadcast_control.models import StageSchema, StageParameter, ParamType
    from broadcast_control.schema_registry import get_stage_registry

    registry = get_stage_registry()
    schema = StageSchema(
        source="ue5_mcp_test",
        version=1,
        parameters=[
            StageParameter(
                name="AR_Hologram_Alpha", type=ParamType.FLOAT,
                default=0.0, min_value=0.0, max_value=1.0,
                tags=["全息", "透明度", "AR"], description="全息投影透明度",
            ),
            StageParameter(
                name="Ambient_Light", type=ParamType.FLOAT,
                default=0.5, min_value=0.0, max_value=1.0,
                tags=["环境光", "亮度"], description="环境光亮度",
            ),
            StageParameter(
                name="Firework_Trigger", type=ParamType.EVENT,
                default=0.0,
                tags=["烟花", "特效", "触发", "转场"], description="烟花特效触发器",
            ),
            StageParameter(
                name="Stage_Fog_Density", type=ParamType.FLOAT,
                default=0.0, min_value=0.0, max_value=1.0,
                tags=["烟雾", "浓度", "氛围"], description="舞台烟雾浓度",
            ),
        ],
    )
    registry.register(schema)

    # get_capabilities
    results = await _handle_get_capabilities({})
    check("MCP get_capabilities 有返回", len(results) > 0)
    text = results[0].text
    check("MCP capabilities 含全息", "AR_Hologram_Alpha" in text)
    check("MCP capabilities 含环境光", "Ambient_Light" in text)
    check("MCP capabilities 含烟花", "Firework_Trigger" in text)
    print(f"     First 300 chars: {text[:300]}...")

    # execute_stage_cue — 语义匹配
    results = await _handle_execute_stage_cue({"cue_name": "淡入全息投影", "value": 0.8})
    check("MCP cue 淡入全息 无错误", True)
    text = results[0].text
    check("MCP cue 匹配 Hologram", "AR_Hologram_Alpha" in text)
    print(f"     {text}")

    # execute_stage_cue — 环境光
    results = await _handle_execute_stage_cue({"cue_name": "将环境光调暗", "value": 0.15})
    check("MCP cue 环境光 无错误", True)
    text = results[0].text
    check("MCP cue 匹配 Ambient", "Ambient_Light" in text)
    print(f"     {text}")

    # execute_stage_cue — 事件触发
    results = await _handle_execute_stage_cue({"cue_name": "触发烟花特效", "value": 1.0})
    check("MCP cue 烟花 无错误", True)
    text = results[0].text
    check("MCP cue 匹配 Firework", "Firework_Trigger" in text)
    print(f"     {text}")

    # set_float_parameter — 精确控制
    results = await _handle_set_float_parameter({"param_name": "Stage_Fog_Density", "value": 0.6})
    check("MCP set_float 无错误", True)
    text = results[0].text
    check("MCP set_float 成功", "✓" in text)
    print(f"     {text}")

    # execute_stage_cue — 无效 cue (UUID 确保无匹配)
    import uuid
    no_match = f"ZXYWQ_NOMATCH_{uuid.uuid4().hex[:8]}"
    try:
        await _handle_execute_stage_cue({"cue_name": no_match, "value": 0.5})
        check("MCP 无效cue 抛异常", False, "应该抛异常")
    except ValueError as e:
        check("MCP 无效cue 抛异常", True)
        print(f"     ValueError: {str(e)[:120]}")


async def test_e2e_pipeline():
    print("\n--- [7] 端到端链路验证 ---")
    from broadcast_control.models import (
        compute_param_hash,
        pack_udp_message,
        unpack_udp_message,
        StageSchema, StageParameter, ParamType,
    )
    from broadcast_control.schema_registry import get_stage_registry
    from broadcast_control.udp_sender import get_broadcaster

    registry = get_stage_registry()

    schema = StageSchema(
        source="ue5_e2e_test",
        version=1,
        parameters=[
            StageParameter(
                name="AR_Hologram_Alpha", type=ParamType.FLOAT,
                default=0.0, min_value=0.0, max_value=1.0,
                tags=["全息", "透明度", "AR", "淡入淡出"], description="全息投影透明度",
            ),
        ],
    )
    registry.register(schema)

    # Step 1: Schema 已缓存
    check("E2E Schema 已缓存", registry.parameter_count == 1)

    # Step 2: 语义匹配
    param = registry.resolve_cue("把全息调亮")
    check("E2E 语义匹配 全息", param is not None and param.name == "AR_Hologram_Alpha")

    # Step 3: Hash 计算
    param_hash = compute_param_hash(param.name)
    check("E2E Hash 非零", param_hash != 0)
    print(f"     AR_Hologram_Alpha → 0x{param_hash:08X}")

    # Step 4: UDP 打包
    pkt = pack_udp_message(param_hash, 0.55)
    check("E2E 包长度=8", len(pkt) == 8, f"got {len(pkt)}")
    h, v = unpack_udp_message(pkt)
    check("E2E 解包 hash 一致", h == param_hash)
    check("E2E 解包 value 一致", abs(v - 0.55) < 0.001)
    print(f"     8字节包: {pkt.hex()}")

    # Step 5: UDP Broadcaster 已连接
    broadcaster = get_broadcaster()
    check("E2E Broadcaster 已连接", broadcaster._connected)

    # Step 6: 发送验证
    ok = await broadcaster.broadcast(param_hash, 0.77)
    check("E2E UDP 发送成功", ok)
    print(f"     已发送: 0x{param_hash:08X} = 0.77 → 127.0.0.1:7000")


async def main():
    global PASS, FAIL
    print("=" * 60)
    print("  虚拟制片播控中台 — 全线功能验证")
    print("=" * 60)

    # 等待后台服务就绪
    print("\n等待服务就绪 ...")
    for i in range(10):
        try:
            async with httpx.AsyncClient() as c:
                resp = await c.get(f"{HTTP_BASE}/health", timeout=2)
                if resp.status_code == 200:
                    print(f"  服务已就绪 (尝试 {i+1})\n")
                    break
        except Exception:
            pass
        await asyncio.sleep(0.5)
    else:
        print("  ✗ 服务未就绪，请先启动: python -m uvicorn broadcast_control.main:app")
        return

    await test_health()
    await test_schema_upload()
    await test_schema_list()
    await test_command_send()
    await test_websocket()
    await test_mcp_tools()
    await test_e2e_pipeline()

    print(f"\n{'=' * 60}")
    print(f"  结果: {PASS} 通过 / {FAIL} 失败 (共 {PASS + FAIL} 项)")
    print(f"{'=' * 60}")

    if FAIL > 0:
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())
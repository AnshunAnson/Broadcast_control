"""验证播控中台核心模块导入与功能。"""
from broadcast_control.models import (
    compute_param_hash,
    pack_udp_message,
    unpack_udp_message,
    StageSchema,
    StageParameter,
    ParamType,
)
from broadcast_control.schema_registry import StageSchemaRegistry
from broadcast_control.config import UDP_TARGET_HOST, UDP_TARGET_PORT

print("=== 1. 模块导入 ===")
print(f"UDP 目标: {UDP_TARGET_HOST}:{UDP_TARGET_PORT}")

print("\n=== 2. FNV-1a Hash ===")
h = compute_param_hash("AR_Hologram_Alpha")
print(f"AR_Hologram_Alpha → 0x{h:08X} ({h})")

print("\n=== 3. UDP 8 字节打包/解包 ===")
pkt = pack_udp_message(0xDEADBEEF, 0.75)
print(f"打包: {pkt.hex()} ({len(pkt)} bytes)")
hash_out, val_out = unpack_udp_message(pkt)
print(f"解包: hash=0x{hash_out:08X} value={val_out}")
assert hash_out == 0xDEADBEEF
assert abs(val_out - 0.75) < 0.0001
print("  ✓ 打包/解包一致性验证通过")

print("\n=== 4. Schema Registry ===")
registry = StageSchemaRegistry()
schema = StageSchema(
    source="ue5",
    version=1,
    parameters=[
        StageParameter(
            name="AR_Hologram_Alpha",
            type=ParamType.FLOAT,
            default=0.0,
            min_value=0.0,
            max_value=1.0,
            tags=["全息", "透明度", "AR", "淡入淡出"],
            description="全息投影透明度"
        ),
        StageParameter(
            name="Ambient_Light",
            type=ParamType.FLOAT,
            default=0.5,
            min_value=0.0,
            max_value=1.0,
            tags=["环境光", "亮度", "灯光"],
            description="环境光亮度"
        ),
        StageParameter(
            name="Firework_Trigger",
            type=ParamType.EVENT,
            default=0.0,
            tags=["烟花", "特效", "触发"],
            description="烟花特效触发器"
        ),
    ],
)
count = registry.register(schema)
print(f"  注册参数: {count} 个")

print("\n=== 5. 精确查找 ===")
p = registry.lookup_exact("AR_Hologram_Alpha")
assert p is not None
print(f"  lookup_exact('AR_Hologram_Alpha') → {p.name} [{p.type}]")

print("\n=== 6. 语义检索 ===")
results = registry.search_semantic("全息投影")
for param, score in results:
    print(f"  '{param.name}' score={score:.3f}")

print("\n=== 7. 智能导播解析 ===")
cue = registry.resolve_cue("淡入全息")
assert cue is not None
print(f"  '淡入全息' → {cue.name}")

cue2 = registry.resolve_cue("调暗环境光")
assert cue2 is not None
print(f"  '调暗环境光' → {cue2.name}")

cue3 = registry.resolve_cue("触发烟花")
assert cue3 is not None
print(f"  '触发烟花' → {cue3.name}")

print("\n=== 全部测试通过 ✓ ===")

from broadcast_control.models import compute_param_hash

# 测试几个参数名
params = [
    "AR_Hologram_Alpha",
    "AR_EmissivePower", 
    "Ambient_Intensity",
    "Event_Firework",
]

print("参数名 → Hash (16进制 → 10进制")
print("-" * 60)
for name in params:
    h = compute_param_hash(name)
    print(f"{name:25} → 0x{h:08X} → {h}")

print("\n✅ 这就是后台发送给 UE5 的值！")


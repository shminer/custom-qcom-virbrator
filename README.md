### 纯AI编写的高通振动控制程序

#### 用法
用法:
  ./virb <device>                        # 运行测试序列
  ./virb <device> simple <duration_ms>          # 简单震动
  ./virb <device> constant <duration_ms> <strength> # 常量震动
  ./virb <device> predefined <effect_id> [strength] # 预定义效果
  ./virb <device> test                          # 运行测试

参数说明:
  device: 输入设备路径, 例如 /dev/input/event4
  duration_ms: 持续时间(毫秒)
  strength: 强度值 (0-32767)
  effect_id: 预定义效果ID

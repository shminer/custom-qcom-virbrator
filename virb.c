#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <errno.h>
#include <stdint.h>

/// 此程序由AI编写，适用于高通震动程序，默认设备路径为 /dev/input/event4
/// 请根据实际情况修改设备路径

// 使用标准Linux输入头文件中的定义，删除重复的宏定义
// 自定义数据结构（根据驱动中的定义）
struct custom_fifo_data
{
    uint32_t idx;
    uint32_t length;
    uint32_t play_rate_hz;
    uint8_t *data;
};

// 常量效果播放函数
int play_constant_effect(int fd, int duration_ms, int magnitude)
{
    struct ff_effect effect;
    struct input_event play, stop;
    int ret;

    // 设置常量效果
    memset(&effect, 0, sizeof(effect));
    effect.type = FF_CONSTANT;
    effect.id = -1;                      // 让驱动自动分配ID
    effect.u.constant.level = magnitude; // 强度 -32767 到 32767
    effect.replay.length = duration_ms;  // 持续时间(ms)
    effect.replay.delay = 0;             // 延迟时间(ms)

    // 上传效果
    ret = ioctl(fd, EVIOCSFF, &effect);
    if (ret < 0)
    {
        perror("上传常量效果失败");
        return -1;
    }

    printf("常量效果ID: %d, 持续时间: %dms, 强度: %d\n",
           effect.id, duration_ms, magnitude);

    // 播放效果
    memset(&play, 0, sizeof(play));
    play.type = EV_FF;
    play.code = effect.id;
    play.value = 1;

    ret = write(fd, &play, sizeof(play));
    if (ret != sizeof(play))
    {
        perror("播放效果失败");
        ioctl(fd, EVIOCRMFF, effect.id);
        return -1;
    }

    // 等待效果播放完成
    usleep(duration_ms * 1000);

    // 停止效果
    memset(&stop, 0, sizeof(stop));
    stop.type = EV_FF;
    stop.code = effect.id;
    stop.value = 0;

    write(fd, &stop, sizeof(stop));

    // 销毁效果
    ioctl(fd, EVIOCRMFF, effect.id);

    return 0;
}

// 播放预定义效果函数
int play_predefined_effect(int fd, int effect_id, int magnitude)
{
    struct ff_effect effect;
    struct input_event play, stop;
    int16_t custom_data[4] = {0}; // 根据驱动中的CUSTOM_DATA_LEN
    int ret;

    // 设置自定义数据
    custom_data[0] = effect_id; // 效果ID
    custom_data[1] = 0;         // 超时秒（由驱动填充）
    custom_data[2] = 0;         // 超时毫秒（由驱动填充）

    // 设置周期性效果
    memset(&effect, 0, sizeof(effect));
    effect.type = FF_PERIODIC;
    effect.id = -1;
    effect.u.periodic.waveform = FF_CUSTOM;
    effect.u.periodic.period = 100;          // 周期
    effect.u.periodic.magnitude = magnitude; // 幅度
    effect.u.periodic.offset = 0;
    effect.u.periodic.phase = 0;
    effect.u.periodic.custom_len = sizeof(custom_data);
    effect.u.periodic.custom_data = (void *)custom_data;
    effect.replay.length = 1000; // 播放长度会被驱动更新
    effect.replay.delay = 0;

    // 上传效果
    ret = ioctl(fd, EVIOCSFF, &effect);
    if (ret < 0)
    {
        perror("上传预定义效果失败");
        return -1;
    }

    printf("预定义效果ID: %d (系统ID: %d), 幅度: %d\n",
           effect_id, effect.id, magnitude);

    // 读取驱动更新的播放时长
    printf("播放时长: %d秒 %d毫秒\n",
           custom_data[1], custom_data[2]);

    // 播放效果
    memset(&play, 0, sizeof(play));
    play.type = EV_FF;
    play.code = effect.id;
    play.value = 1;

    ret = write(fd, &play, sizeof(play));
    if (ret != sizeof(play))
    {
        perror("播放效果失败");
        ioctl(fd, EVIOCRMFF, effect.id);
        return -1;
    }

    // 等待效果播放完成
    int total_ms = custom_data[1] * 1000 + custom_data[2];
    if (total_ms > 0)
    {
        usleep(total_ms * 1000);
    }
    else
    {
        usleep(1000000); // 默认等待1秒
    }

    // 停止效果
    memset(&stop, 0, sizeof(stop));
    stop.type = EV_FF;
    stop.code = effect.id;
    stop.value = 0;

    write(fd, &stop, sizeof(stop));

    // 销毁效果
    ioctl(fd, EVIOCRMFF, effect.id);

    return 0;
}

// 播放自定义FIFO数据
int play_custom_fifo_effect(int fd, uint8_t *fifo_data, uint32_t data_length,
                            uint32_t play_rate_hz, int magnitude)
{
    struct ff_effect effect;
    struct input_event play, stop;
    struct custom_fifo_data custom_data;
    int ret;

    // 设置自定义FIFO数据
    custom_data.idx = 0;
    custom_data.length = data_length;
    custom_data.play_rate_hz = play_rate_hz;
    custom_data.data = fifo_data;

    // 设置周期性效果
    memset(&effect, 0, sizeof(effect));
    effect.type = FF_PERIODIC;
    effect.id = -1;
    effect.u.periodic.waveform = FF_CUSTOM;
    effect.u.periodic.period = 100;
    effect.u.periodic.magnitude = magnitude;
    effect.u.periodic.offset = 0;
    effect.u.periodic.phase = 0;
    effect.u.periodic.custom_len = sizeof(custom_data);
    effect.u.periodic.custom_data = (void *)&custom_data;
    effect.replay.length = 1000;
    effect.replay.delay = 0;

    // 上传效果
    ret = ioctl(fd, EVIOCSFF, &effect);
    if (ret < 0)
    {
        perror("上传自定义FIFO效果失败");
        return -1;
    }

    printf("自定义FIFO效果ID: %d, 数据长度: %d, 播放速率: %dHz\n",
           effect.id, data_length, play_rate_hz);

    // 播放效果
    memset(&play, 0, sizeof(play));
    play.type = EV_FF;
    play.code = effect.id;
    play.value = 1;

    ret = write(fd, &play, sizeof(play));
    if (ret != sizeof(play))
    {
        perror("播放效果失败");
        ioctl(fd, EVIOCRMFF, effect.id);
        return -1;
    }

    // 等待效果播放完成（这里需要根据实际情况调整）
    usleep(2000000);

    // 停止效果
    memset(&stop, 0, sizeof(stop));
    stop.type = EV_FF;
    stop.code = effect.id;
    stop.value = 0;

    write(fd, &stop, sizeof(stop));

    // 销毁效果
    ioctl(fd, EVIOCRMFF, effect.id);

    return 0;
}

// 简单震动函数 - 最基础的测试
int simple_vibrate(int fd, int duration_ms)
{
    struct ff_effect effect;
    struct input_event play, stop;
    int ret;

    // 创建简单的常量效果
    memset(&effect, 0, sizeof(effect));
    effect.type = FF_CONSTANT;
    effect.id = -1;
    effect.u.constant.level = 0x7FFF; // 最大强度
    effect.replay.length = duration_ms;
    effect.replay.delay = 0;

    ret = ioctl(fd, EVIOCSFF, &effect);
    if (ret < 0)
    {
        perror("创建震动效果失败");
        return -1;
    }

    printf("简单震动: %dms\n", duration_ms);

    // 播放
    memset(&play, 0, sizeof(play));
    play.type = EV_FF;
    play.code = effect.id;
    play.value = 1;

    ret = write(fd, &play, sizeof(play));
    if (ret != sizeof(play))
    {
        perror("播放震动失败");
        ioctl(fd, EVIOCRMFF, effect.id);
        return -1;
    }

    // 等待
    usleep(duration_ms * 1000);

    // 停止
    memset(&stop, 0, sizeof(stop));
    stop.type = EV_FF;
    stop.code = effect.id;
    stop.value = 0;

    write(fd, &stop, sizeof(stop));
    ioctl(fd, EVIOCRMFF, effect.id);

    return 0;
}

// 测试函数
void test_vibration(int fd)
{
    printf("=== 测试简单震动 ===\n");
    simple_vibrate(fd, 500); // 500ms震动

    usleep(300000); // 等待300ms

    printf("=== 测试常量震动 ===\n");
    play_constant_effect(fd, 1000, 0x4000); // 1秒，中等强度

    usleep(500000); // 等待500ms

    printf("=== 测试预定义效果 ===\n");
    play_predefined_effect(fd, 1, 0x6000); // 效果ID 1，中等强度
}

// 检查设备支持的功能
void check_device_capabilities(int fd)
{
    unsigned long features[4];
    int ret;

    ret = ioctl(fd, EVIOCGBIT(EV_FF, sizeof(features)), features);
    if (ret < 0)
    {
        perror("获取设备功能失败");
        return;
    }

    printf("设备支持以下力反馈效果:\n");
    if (features[0] & (1 << FF_CONSTANT))
        printf("  - 常量效果 (FF_CONSTANT)\n");
    if (features[0] & (1 << FF_PERIODIC))
        printf("  - 周期效果 (FF_PERIODIC)\n");
    if (features[0] & (1 << FF_CUSTOM))
        printf("  - 自定义效果 (FF_CUSTOM)\n");

    // 检查其他支持的效果
    const char *effect_names[] = {
        "FF_RUMBLE", "FF_SPRING", "FF_FRICTION", "FF_DAMPER",
        "FF_INERTIA", "FF_RAMP", "FF_SQUARE", "FF_TRIANGLE",
        "FF_SINE", "FF_SAW_UP", "FF_SAW_DOWN"};
    int effect_ids[] = {
        0x50, 0x52, 0x53, 0x54, 0x55, 0x56, 0x58, 0x59,
        0x5a, 0x5b, 0x5c};

    for (int i = 0; i < 11; i++)
    {
        if (features[0] & (1 << effect_ids[i]))
        {
            printf("  - %s\n", effect_names[i]);
        }
    }
}

int main(int argc, char *argv[])
{
    int fd;
    int ret;
    const char *device_path;

    if (argc < 2)
    {
        printf("用法:\n");
        printf("  %s <device>                        # 运行测试序列\n", argv[0]);
        printf("  %s <device> simple <duration_ms>          # 简单震动\n", argv[0]);
        printf("  %s <device> constant <duration_ms> <strength> # 常量震动\n", argv[0]);
        printf("  %s <device> predefined <effect_id> [strength] # 预定义效果\n", argv[0]);
        printf("  %s <device> test                          # 运行测试\n", argv[0]);
        printf("\n参数说明:\n");
        printf("  device: 输入设备路径, 例如 /dev/input/event4\n");
        printf("  duration_ms: 持续时间(毫秒)\n");
        printf("  strength: 强度值 (0-32767)\n");
        printf("  effect_id: 预定义效果ID\n");
        return -1;
    }

    // 第一个参数作为设备路径
    device_path = argv[1];

    // 打开输入设备
    fd = open(device_path, O_RDWR);
    if (fd < 0)
    {
        perror("打开输入设备失败");
        printf("请检查设备路径 %s 是否存在\n", device_path);
        printf("可能需要root权限运行\n");
        return -1;
    }

    // 检查设备是否支持力反馈
    unsigned long features[4];
    ret = ioctl(fd, EVIOCGBIT(EV_FF, sizeof(features)), features);
    if (ret < 0)
    {
        perror("设备不支持力反馈或权限不足");
        close(fd);
        return -1;
    }

    printf("设备 %s 支持力反馈功能\n", device_path);
    check_device_capabilities(fd);

    // 如果没有额外命令参数，则运行测试
    if (argc >= 3)
    {
        // 剩余参数作为命令处理
        if (strcmp(argv[2], "simple") == 0 && argc >= 4)
        {
            int duration = atoi(argv[3]);
            simple_vibrate(fd, duration);
        }
        else if (strcmp(argv[2], "constant") == 0 && argc >= 5)
        {
            int duration = atoi(argv[3]);
            int strength = atoi(argv[4]);
            play_constant_effect(fd, duration, strength);
        }
        else if (strcmp(argv[2], "predefined") == 0 && argc >= 4)
        {
            int effect_id = atoi(argv[3]);
            int strength = (argc >= 5) ? atoi(argv[4]) : 0x7FFF;
            play_predefined_effect(fd, effect_id, strength);
        }
        else if (strcmp(argv[2], "test") == 0)
        {
            test_vibration(fd);
        }
        else
        {
            printf("未知命令。\n", argv[0]);
        }
    }

    close(fd);
    return 0;
}
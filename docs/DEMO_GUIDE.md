# 5分钟演示与复现

## 0:00–1:00 单板独立启动

用户自行插接LCD、烧录并复位。约1.5秒后进入中文主界面：标题“主动视觉跟踪系统”、`ActiveVisionTracking`、待机/离线、运行时间、无有效目标、计数与按键说明。DS0待机熄灭。未接PC不伪造视频数据。

短按KEY0：待机→运行，DS0常亮；无PC且从未接收过目标时显示“本地生成模拟轨迹”，目标区有明确标记的模拟十字。再短按→暂停，DS0每500ms翻转，虚拟控制归零；再短按恢复；长按约1秒回到待机。不接触任何电机。

## 1:00–1:40 LCD色块自检

按住KEY0后按下/释放RESET，再松开KEY0。上电时采样到KEY0按下，进入红绿蓝白黑色块、中文标题与四角坐标标记。约1秒长按事件被自检消费，不自动退出；重新短按KEY0退出到待机。先验证自检，再看主界面。

## 1:40–3:20 PC真实算法演示

```powershell
cd D:\STM32Projects\ActiveVisionTracking
.\.venv\Scripts\python.exe -m host --frames 1800 --delay-ms 120 --jitter-ms 20 --loss 0.03 --output runs\gui
```

无摄像头、无外设、无大模型下载。绿色目标由合成图像生成；检测器只接收像素，不读取真值。白圈=目标真值，黄圈=检测中心，青圈=未来位置。跟踪使用颜色连通域与常速度卡尔曼滤波，短时遮挡继续预测，丢失超过0.65秒停止发送有效控制，出现后重捕获。

每12秒周期：3.00–3.55秒完全遮挡；5.0–5.4秒部分遮挡；7.0–8.8秒长遮挡。此段应经历跟踪→遮挡→丢失→重新捕获→跟踪。P键切换补偿显示与发送位置；Q/ESC退出。串口模式R键重试实际串口。

默认后端是**原生编译的C固件核心**，经过真实二进制协议回传状态，中文窗口明确显示“C固件核心软件仿真 / 未连接真实开发板”。日志`trace.csv`与`summary.json`、`preview.png`和遮挡预览`occlusion.png`写入输出目录。CSV带后端标签，不能将仿真MCU收包数当实机证据。

自动验证：

```powershell
.\.venv\Scripts\python.exe -m host --headless --frames 600 --output runs\headless
.\.venv\Scripts\python.exe -m pytest tests -q
```

summary中的补偿开/关误差是在**同一批到达事件**与合成真值上配对比较的位置误差，不是实体云台误差，也不是准确率基准。主机处理p95来自实际执行，受PC负载影响，不保证每次相同。

可选本地视频：`--video D:\路径\video.mp4`，默认找绿色目标；用`--roi X Y W H`在第一帧指定有明显饱和颜色的目标区域提取颜色。它是单目标颜色跟踪器，对相同颜色干扰、复杂背景和长遮挡中的身份保持有局限；没有实现通用目标识别/多目标ReID。

## 3:20–4:40 实机串口联动（仅接线核对后）

按HARDWARE_WIRING文档连接板载USB串口、检查桥TX/RX跳线与COM。关闭其他占用串口的软件。

```powershell
.\.venv\Scripts\python.exe -m host --list-ports
.\.venv\Scripts\python.exe -m host --backend serial --port COM实际号 --frames 1800 --output runs\serial
```

LCD按KEY0进入运行，预期显示电脑合成视频、目标当前/预测坐标、遮挡状态、模拟时延、RX/TX/CRC与虚拟H/V。PC应显示SERIAL且收到MCU状态。如果端口异常自动回到仿真，界面标签会改变；这时不能宣称联动成功。

## 4:40–5:00 故障保护

拔掉数据线或退出PC，LCD在目标剩余有效期用尽后显示离线，虚拟H/V归零；暂停状态同样归零。复接重握手后新数据恢复。屏幕断电/错接、串口占用等硬件问题不会由主机仿真证明已解决。

# ActiveVisionTracking 二进制协议 v1

所有多字节整数小端；目标坐标使用归一化无符号整数 0..10000，图像左上为原点，中心 5000。串口 115200、8N1、无流控。一个目标包 38 字节，30 Hz 约 1140 B/s。

| 帧内偏移 | 字段 | 大小 |
|---|---|---|
|0|A5 5A 帧头|2|
|2|版本=1|1|
|3|消息类型|1|
|4|payload 长度 0..48|2|
|6|seq|2|
|8|发送方毫秒时间戳|4|
|12|payload|0..48|
|12+长度|CRC16|2|

CRC16/CCITT-FALSE：poly 0x1021、init 0xFFFF、refin/refout=false、xorout=0；覆盖从版本到 payload 末尾，CRC 本身按小端存放。`123456789` 的 CRC=0x29B1。

固定 PING 向量（type=2，seq=0x1234，stamp=0x01020304，token=0x11223344）：

`a5 5a 01 02 04 00 34 12 04 03 02 01 44 33 22 11 dc 6c`

已与 C、Python 和标准库 `binascii.crc_hqx` 三者交叉核对。

## 会话与消息

- HELLO=0x03 / HELLO_ACK=0x83：4 字节非零随机 session。新 session 清除旧目标及序列窗口。同一 session 的重复握手不重置序列号。每次重新启动 PC 进程使用新 session。
- PING=0x02 / PONG=0x82：4 字节 token，原样回显；seq 原样回显，响应 header.stamp 使用 MCU tick。PC 仅用自己的发包/收到回包时间计算 RTT。不同设备时钟不能相减作为单程时延。
- TARGET=0x01：24 字节 payload，见下表。
- STATUS=0x81：32 字节 payload，MCU 每 200 ms 发送，即便没有 PC。PC 仿真后端为了便于观察每轮可返回一次状态。

| TARGET payload 偏移 | 字段 |
|---|---|
|0|session u32|
|4|flags u8：bit0=目标有效；bit1=合成输入；其他位必须为0|
|5|track u8：0跟踪 / 1短时遮挡 / 2丢失 / 3重捕获|
|6|confidence u16，0..1000|
|8,10|当前 x,y，各u16，0..10000|
|12,14|预测 x,y，各u16，0..10000|
|16|注入的模拟时延 ms，u16；不是实测单程延迟|
|18|有效期 ttl_ms，u16，50..1000|
|20|age_ms，u16，发送前已知的数据年龄，必须小于ttl|
|22|保留 u16=0|

有效性同时要求 flags.bit0、置信度≥150、track!=LOST。主机在重捕获的前两帧主动发送 invalid。固件不会使用过期/无效目标驱动虚拟量。RUNNING 状态之外虚拟输出强制为0。虚拟 H/V 只在内存与屏幕中使用，范围±1000，无 PWM/电机输出。

STATUS payload：0 state u8（0待机、1运行、2暂停、3故障），1 flags u8（bit0 online、bit1 valid），2 fault u16（0无、1LCD探测失败、2时钟配置不符），4 accepted u32，8 rejected u32，12 CRC错误u32，16 duplicate u32，20 timeout u32，24 H i16，26 V i16，28 last_target_seq u16，30 firmware_version u16（0x0100）。串口溢出、硬件错误、TX计数、TIM计数在屏幕/调试变量中查看。

## 失联与错误

- 流式处理半包/粘包；错误帧逐字节重新寻找帧头；长度>48、版本错误立即重同步。半包超过100ms放弃。
- 仅合法消息刷新目标 TTL；CRC错包、字段错误、未知类型、旧 session 均拒绝。
- seq 使用16位模数比较，窗口32768；重复及乱序包不刷新 TTL；65535→0正常。
- MCU 以本地接收tick加 `ttl-age` 维护剩余有效期，避免跨设备时钟比较；接收后剩余有效期用尽即离线、数据失效、虚拟量立即归零。
- age包含 PC 延时队列里的等待，串口线上的额外等待没有时钟同步，不能声称被精确测量；115200/38字节约3.3ms，实际USB驱动排队仍需上板观察。TTL是已知年龄基础上的保护，不能抵御未知的长时间外部缓存。
- UART ISR 只搬字节并计数。RX环1024 B，TX环512 B，TX空间不足整帧丢弃并计数。无阻塞打印。
- serial 无端口、被占用、I/O异常自动切到明确标记的 C 软件仿真；静默无回包超过1秒标离线并重握手；GUI按R可手动重试串口。

实现：`User/Src/avt_protocol.c`、`avt_core.c`、`host/protocol.py`；可运行验证：`.venv\Scripts\python -m pytest tests -q`。

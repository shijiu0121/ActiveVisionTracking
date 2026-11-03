# 仅使用已确认硬件的连接

硬件：STM32F103ZET6战舰底板ATK-DNF103P V4.7、ATK-MD0700 V2.4、ST-LINK、Windows PC。没有真实电机/舵机/云台输出。

## LCD与供电

断电插接屏幕。模块是17x2接口，P1-1=CS，2=RS，3=WR，4=RD，5=RST，6..21=D0..15，22/26/27=GND，23=TE，28=5V；24/25/32未用，29..34为触摸相关线路。用模块/底板原理图核对1脚与供电方向后再上电，不能凭接口外形猜方向。

屏幕为5V供电、3.3V逻辑，SSD1963内部提供显存；不要连接到单纯RGB屏接口。V3或RGB V3型号的1024x600屏不属于本项目支持范围。保持SWD PA13/PA14与原ST-LINK连接方案。不要同时由多个未经核对的电源向板子反向供电。

## 板载USB串口

厂商战舰V4教程确认USART1 PA9/PA10连接板载CH340。**当前没有打开任何真实COM端口，也没有声称联调成功。** 请使用底板丝印表示`USB串口 / USB UART / USB_232`的电脑接口；`USB SLAVE`及ST-LINK口不能默认当此数据口。

若有串口桥接选择跳线，核对网名：**桥TXD→MCU PA10/RX；桥RXD→MCU PA9/TX**。V4.7具体跳线位号/针序尚未获得对应底板图纸确认，因此本文不沿用V3资料的P4编号。不要根据本文猜测跳线帽朝向。保持正常Flash启动设置BOOT0=0；上位机关闭DTR/RTS，不调用ISP下载功能。

```powershell
cd D:\STM32Projects\ActiveVisionTracking
.\.venv\Scripts\python.exe -m host --list-ports
```

插入/拔出USB串口线前后对比端口列表，确认新增CH340对应COM号。不要沿用其他开发板曾用的COM9。按实际值替换：

```powershell
.\.venv\Scripts\python.exe -m host --backend serial --port COM实际号 --frames 1800
```

115200，8N1，无流控；关闭其他串口助手以免端口占用。LCD开机待机，**短按KEY0进入运行**后虚拟H/V才会变化；接收显示在待机/暂停时仍工作。PC窗口必须显示SERIAL且有MCU回包计数，才算真实链路；如果显示“软件仿真”，表示已自动回退，不能作为串口成功证据。

板子不接PC时仍可启动；运行态、尚未收到PC数据时显示“本地模拟”，仅演示屏上轨迹。已接收过PC后失联显示“离线”，不以本地模拟掩盖断线。

# Node-RED简介
Node-RED是一款将硬件设备/API和在线服务以一种新鲜有趣的方式连接整合一体的编程工具. Node-RED为物联网开发提供极大的便利性. 有关Node-RED更多的介绍请看[Node-RED教程](https://www.jianshu.com/p/c30cf0362821). 本控制器基于Node-RED中的MQTT与Dashboard实现, 开发时间约2小时, 足见其方便快捷.

# OpenBot简介
```
OpenBot 利用智能手机作为低成本机器人的大脑。我们设计了一款小型电动车，各种配件加一起售价约 50 美元，可以用作机器人车身。 我们的 Android 智能手机软件栈支持先进的机器人功能，如人跟踪和实时自主导航。
```
以上是官方简介, 大致意思是以Android手机核心, 通过串口与Arduino nano通信, 从而实现一个智能小车. 由于Android手机的引入, 我们可以基于许多成熟的技术栈(TensorFlow lite/OpenCV等)实现许多较为先进的功能(人跟随/自主导航, 也不是特别先进 :r). 
关键在于, 如果使用一款闲置的Android手机, 小车的成本据官方说,是小于50美元(大约300人民币). 但实际上, 在China!!!, 这个成本可以压缩到100元左右(不包括手柄). 低廉的成本和开放的软件生态, 使这个项目非常耐玩.

# 控制器的具体实现
OpenBot支持多种控制模式:
1. 无手机模式 - 要在Arduino 固件中修改宏定义
这个模式就是简单的避障小车, 前方有障碍物随机转向.
2. 含手机模式
将手机最为Arduino的主控, 通过串口通信. 同时, 该模式又有几种控制方式.
a. 手柄控制
用蓝牙手柄连接到Android机, 并切换到Gamepad, 选择Joystick就可以实现用手柄控制小车运动. 同时手柄多余的按键还可以控制OpenBot的行为模式, 比如: 切换Log模式, 切换控制方式等
b. 手机控制(套娃模式)
假如还有一台Android手机, 你可以将这一台Android手机作为控制器, 控制小车上的Android手机. 但是, 条件对于国内的同学们比较苛刻, 两台手机之间的互联基于Google的NearBy Connection API(底层是蓝牙+WiFi, 与AirDrop类似), 需要Google Play框架, 还要会科学上网. 任何一个都不是说很好实现.遂放弃.
c. WebRTC模式
该模式比较理想, 可以实现FPV. 但是但是, 也只是在程序的注释中看到了, 还没有完全实现, 期待Intel官方的push.
d. MQTT模式(个人实现)
MQTT老熟人了, 万物皆可MQTT, 虽然在这个项目中MQTT并不是特别合适, 毕竟MQTT是Pub-Sub模式, 而我们这个场景是P2P场景, 不会存在一台PC控制多台小车的情况(当然也可以). 但作为一种通信手段, 勉为其难的用了吧.

## Android端
可能是Intel官方考虑到了多种平台的控制, 预留出了足够的修改余地. 这使得增加控制模式非常方便. 参照官方的[文档](https://github.com/bing-zhub/OpenBot/blob/master/docs/technical/OpenBotController.pdf), 我们可以清楚的了解到控制器与小车之间的数据通信.
![38489-6vb7irttwuf.png](http://assets.bingware.cn/2021/02/3833656966.png)
首先仿照`PhoneController`的API复刻出一个`MQTTController`完成建立连接/断开连接/接收数据/发送数据等接口.
在`MQTTController`中定义了两个topic: `SUB_MQTT_TOPIC`与`PUB_MQTT_TOPIC` 分别对应控制器向小车发送数据与小车向控制器发送数据.
![71812-kexf3zpjkl.png](http://assets.bingware.cn/2021/02/3803376511.png)
### 小车向控制器发送状态数据
项目中使用了Event Bus, 所有有关数据都会放到上面, 在MQTTController中, 订阅即可获取数据.
``` java
BotToControllerEventBus.getProcessor().subscribe(event -> send(event));
```
数据整体流向 Arduino nano --Serial--> onSerialDataReceived --LocalBroadcast--> CameraActivity中的localBroadcastReceiver 
但获取的数据与预期不一致, 有些复杂, 比较麻烦, 遂在`UsbConnection`中`onSerialDataReceived`方法直接将从串口读取到的数据发送至控制器.在Node-RED中再次解析.

### 控制器向小车发送控制命令
在[文档](https://github.com/bing-zhub/OpenBot/blob/master/docs/technical/OpenBotController.pdf)中, 
较为详细的写了命令下发的过程. 按照文档修改MQTT包中payload即可.

## Node-RED端
![58037-xgj2etk9kd.png](http://assets.bingware.cn/2021/02/2399567414.png)
![67690-s1dghk8l5kc.png](http://assets.bingware.cn/2021/02/2433294685.png)
总体来看比较直接, 主要就是接口适配, 调整一下数据格式.
通过MQTT获取小车数据, 进行简单的解析, 对应到Dashboard的Gauge节点即可.
小车的运动控制使用Dashboard中的Template节点实现. 通过`Event Listener`监听全局`keydown`与`keyup`事件, 给`W`/`A`/`S`/`D`/`R`键绑定不同的数据, 控制小车运动方向. 


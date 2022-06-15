# Vision C3 Youth 
基于 ESP32-C3 的下北泽神之眼青春版  
这里包含可以直接替换 [小渣渣](https://space.bilibili.com/14958846) 的圆版及方版核心板的 PCB 设计及其代码  
你也可以看看这个设计在经历两次缩水前的全功能版是什么样子（雾）： [Github: libc0607/esp32-vision](https://github.com/libc0607/esp32-vision)  
[Bilibili: 下北泽元素力青春版核心板](https://www.bilibili.com/video/BV1J94y1U7n7)  
![demo240](https://user-images.githubusercontent.com/8705034/172422163-bd384d19-6873-4483-bdf5-e652954234d6.gif)

与原设计相比，这里的设计目标：
 - 直接替换他的电路板，且其他部件完全兼容
 - 在基本保持与小渣渣的设计功能相同的情况下，对硬件进行了大幅缩水  
 - 尽可能降低小批量手工制作的元件成本和采购成本  
 - 全 0603 封装，烙铁友好  
 - 保持 PCB 可白嫖的工艺参数， 5mil/5mil线，0.6/0.3孔  
 - 直接使用 USB 下载，避免消耗串口模块  
 - 缩掉 TF 卡（位置预留了且硬件上验证过可用），使用 ESP32-C3 内部的一点点存储空间
 - 增加了 Wi-Fi 上传，且不需要外接天线即可使用无线功能   
 - 增加了按键切换和蓝牙遥控切换目录下全部视频的功能，视频可无缝循环  
 - 保持 指示灯/按键/Type-C接口/TF卡槽 位置，直接使用 小渣渣 的外壳  
 - 基于 GPL 3.0 协议（可商用）开源，适合自己做点拿去卖


## 硬件设计
方版见 [OSHWHub: 璃月核心板 C3 青春版（？）](https://oshwhub.com/libc0607/liyue-c3-lowcost-v1)   
圆版见 [OSHWHub: 神之眼 C3 青春版核心板 V1](https://oshwhub.com/libc0607/vision-c3-youth-gc9a01-v1)   
在本仓库内也上传了一份，包括 iBom  

## 外壳
直接使用 [小渣渣的璃月版神之眼](https://www.bilibili.com/video/BV1HS4y1b7tQ) 资料中提到的外壳即可  
他的外壳基于我的版本修改，你也可以自己改改，不爽了也可以自己照着外形画一个  

## 这堆代码怎么用
写在代码前面的注释里了，懒得再写一遍了  
不带蓝牙版大概有 2900 kB 可用空间，蓝牙版大约 1900 kB；七元素各存几秒，并做一下循环的头尾衔接，还是够用的  

目前的代码包含两个 Bug：
1. 蓝牙断开连接的时候会导致本体自动关机  
  这个问题大概和上游代码有关，因为同样的代码在 esp32 上就能用；不过暂时可以当作 feature 来用，遥控关机嘛（不是  
2. 电池电压读取有问题，应该能读，懒得试了  

烧写之前你可能需要使用乐鑫提供的 flash download tool 进行清空操作  
如果无法下载或是循环重启，尝试使用不同的 arduino-esp32 版本  

## 参考
视频播放部分参考 [moononournation/RGB565_video](https://github.com/moononournation/RGB565_video)  
蓝牙 iTag 部分参考 [100-x-arduino.blogspot.com](http://100-x-arduino.blogspot.com/) 中给出的示例进行修改  
如果你需要购买 iTag，我在淘宝上买的 [这个](https://item.taobao.com/item.htm?id=556798481873)，但我不认识卖家（反正就十块多的东西  
网页上传部分参考自 arduino-esp32 中的 SDWebServer 示例  
元素图标来自 [Bilibili: 鱼翅翅Kira](https://space.bilibili.com/2292091)  


## 协议 & 免责声明 
本设计由于基于小渣渣的设计修改而来，采用同样的 GPL 3.0 协议授权  
你可以在这里的基础上进行修改，可分享，可商用，但是修改后要开源  
虽然该协议可以商用，但不代表作者支持你的行为，作者也不会对商业化造成的麻烦承担任何责任；作者本人也没有在任何平台卖成品或套件，有问题看代码请别找我售后谢谢  
作者与 mhy 没什么关系，如果你需要商用，请参照 [《原神同人周边大陆地区正式运行指引V1.2》](https://weibo.com/ttarticle/p/show?id=2309404707028085113324) （不保证本链接为最新版，请自行查看）进行  

BLE 版本代码中引用了 [stevemarple/IniFile](https://github.com/stevemarple/IniFile)   
由于需要从 FFat 加载配置文件，而原库不支持，所以对库做了一点点修改；为了不影响环境，将其作为附带文件加入  
故 IniFile.cpp 和 IniFile.h 基于与原项目相同的 LGPL 2.1 开源  

作者不对本设计的可靠性及其可能造成的损失负责，请自行评估风险； 
作者对原创部分保留最终解释权利   
作者目前仅验证了这堆代码的基本功能，处于开源后摆烂的状态；   
作者会在一定范围内对这个设计的相关疑问做出解答，严重 Bug 有可能会修，但没有义务上的保证，更没有义务给别人搞售后；  
如果你想要了解更多，可以加 小渣渣 的 QQ 群 636426429  


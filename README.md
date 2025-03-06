# 1、V4L2应用编程示例
本项目主要收集编写了基于嵌入式Linux下的V4L2应用程序示例。

## uvc_to_jpg.c
* 该程序以一个支持MJPEG格式输出的USB摄像头为对象。
* 采集摄像头数据，并保存为一张张的.jpg图片。
* 展现了单平面视频采集的V4L2编程示例。

## uvc_to_lcd.c
* 该程序以一个支持MJPEG格式输出的USB摄像头为对象。
* 采集摄像头数据，并实时预览在LCD上。
* 包含jpeg解码，jpeg转rgb8888。
* 展现了单平面视频采集的V4L2编程示例。

## mipi_to_yuv.c
* 该程序以一个支持NV21格式输出的MIPI摄像头为对象。
* 采集摄像头数据，并保存为一帧帧的.yuv视频帧文件。
* 展现了多平面视频采集的V4L2编程示例。

# 2、获取程序
```shell
git clone git@github.com:Cohen0415/v4l2_app.git
```

# 3、编译运行程序
## uvc_to_jpc.c
1. 将一个支持MJPEG格式输出的UVC摄像头插到单板上。
2. 如果你使用的是buildroot系统，需要交叉编译。这里用跑Ubuntu的单板来验证，执行如下命令编译程序：
```shell
sudo gcc -o uvc_to_jpg uvc_to_jpg.c
```
3. 执行如下命令运行程序：
```shell
sudo ./uvc_to_jpg /dev/video10
```
4. 程序运行过程中，可以按Ctrl+c退出程序。
5. .jpg文件会被保存在当前目录。

## uvc_to_lcd.c
1. 将一个支持MJPEG格式输出的UVC摄像头插到单板上。
2. 将lcd屏连接到单板，并确保你的lcd屏可以正常驱动。
3. 如果你使用的是buildroot系统，需要交叉编译。这里用跑Ubuntu的单板来验证，执行如下命令编译程序：
```shell
sudo gcc -o uvc_to_lcd uvc_to_lcd.c -ljpeg
```
4. 执行如下命令运行程序：
```shell
sudo ./uvc_to_lcd /dev/video10 /dev/fb0
```

## mipi_to_yuc.c
1. 将一个支持NV21格式输出的MIPI摄像头连接到单板。
2. 如果你使用的是buildroot系统，需要交叉编译。这里用跑Ubuntu的单板来验证，执行如下命令编译程序：
```shell
sudo gcc -o mipi_to_yuc mipi_to_yuc.c
```
3. 执行如下命令运行程序：
```shell
sudo ./mipi_to_yuc /dev/video0
```
4. 程序运行过程中，可以按Ctrl+c退出程序。
5. .yuv文件会被保存在当前目录，可以使用yuv播放器查看。

/*
*
*   file: uvc_to_lcd.c
*   date: 2025-02-26
*   usage: 
*       sudo gcc -o uvc_to_lcd uvc_to_lcd.c
*       sudo ./uvc_to_lcd /dev/videoX /dev/fbX
*
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>          
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/mman.h>
#include <jpeglib.h>
#include <linux/fb.h>

typedef struct lcd_mes {
    int fd;
    unsigned char *fb_base;
    int lcd_width;
    int lcd_height;
    unsigned int bpp;
    unsigned int line_width;
} lcd_mes;

typedef struct camera_mes {
    int fd;
    void *bufs[32];
    int bufs_index;
    int buf_length;
    char capability[20];
    int frame_x_size;
    int frame_y_size;
} camera_mes;

int jpeg_show_on_lcd(lcd_mes *lcd, camera_mes *camera)
{
    int min_width, min_height;
    int valid_bytes;
    int offset_x, offset_y;

	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
    
	cinfo.err = jpeg_std_error(&jerr);      // 错误处理对象与解码对象绑定
	jpeg_create_decompress(&cinfo);         // 初始化解码器
	jpeg_mem_src(&cinfo, camera->bufs[camera->bufs_index], camera->buf_length);   // 指定JPEG数据的来源

	jpeg_read_header(&cinfo, TRUE);         // 读取图像信息
	cinfo.out_color_space = JCS_RGB;        // 设置解码后的颜色空间为RGB

	jpeg_start_decompress(&cinfo);          // 开始解码
	
	unsigned char *jpeg_line_buf = (char *)malloc(cinfo.output_components * cinfo.output_width);    // 用于存储从JPEG解码器读取的一行数据
	unsigned int *fb_line_buf = (int *)malloc(lcd->line_width);                                     // 用于存储转换后的RGB数据，准备写入framebuffer

    min_width = (cinfo.output_width < lcd->lcd_width) ? cinfo.output_width : lcd->lcd_width;
    min_height = (cinfo.output_height < lcd->lcd_height) ? cinfo.output_height : lcd->lcd_height;

	valid_bytes = min_width * lcd->bpp / 8;             // 一行的有效字节数
	unsigned char *ptr = lcd->fb_base;

    offset_x = ((lcd->lcd_width - min_width) / 2) * lcd->bpp / 8;   // x方向居中
    offset_y = (lcd->lcd_height - min_height) / 2;      // y方向居中
    for (int i = 0; i < offset_y; i++)                  
        ptr += lcd->lcd_width * lcd->bpp / 8;
    
    unsigned int red, green, blue;
    unsigned int color;
	while (cinfo.output_scanline < min_height)
	{
		jpeg_read_scanlines(&cinfo, &jpeg_line_buf, 1); // 每次读取一行

		for(int i = 0; i < min_width; i++)              
		{
			red = jpeg_line_buf[i * 3];                 
			green = jpeg_line_buf[i * 3 + 1];           
			blue = jpeg_line_buf[i * 3 + 2];            
			color = red << 16 | green << 8 | blue;      // RGB888转RGB8888

			fb_line_buf[i] = color;
		}

		memcpy(ptr + offset_x, fb_line_buf, valid_bytes);   // 将一行数据写入framebuffer
		ptr += lcd->lcd_width * lcd->bpp / 8;               // 移动到下一行
	}
	
	jpeg_finish_decompress(&cinfo);                     // 完成解码
	jpeg_destroy_decompress(&cinfo);                    // 销毁解码对象
	free(jpeg_line_buf);                                // 释放内存
	free(fb_line_buf);                                  // 释放内存

    return 0;
}

int lcd_init(const char *fb_dev, lcd_mes *lcd)
{
    
    int screen_size;
    struct fb_var_screeninfo var;   

    if (fb_dev == NULL)
        goto _err;

    /* 1. open /dev/fb* */    
	lcd->fd = open(fb_dev, O_RDWR);
	if(lcd->fd < 0)
	{
        printf("can not open %s\n", fb_dev);
        goto _err;
    }

	/* 2. get lcd message */
	if (ioctl(lcd->fd, FBIOGET_VSCREENINFO, &var))
	{
		printf("can not get var\n");
		goto _err;
	}

    screen_size = var.xres * var.yres * var.bits_per_pixel / 8;
    lcd->line_width  = var.xres * var.bits_per_pixel / 8;
	lcd->lcd_width = var.xres;
	lcd->lcd_height = var.yres;
	lcd->bpp = var.bits_per_pixel;
	lcd->fb_base = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, lcd->fd, 0);
	if (lcd->fb_base == (unsigned char *)-1)
    {
        printf("can not mmap\n");
        goto _err;
    }

    memset(lcd->fb_base, 0x00, screen_size);
    return 0;

_err:
    return -1;
}

int camera_init(const char *video, camera_mes *camera)
{   
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum fsenum;
    int fmt_index = 0;
    int frame_index = 0;
    int buf_cnt;
    int i;

    if (video == NULL)
        goto _err;

    /* 1. open /dev/video* */
    camera->fd = open(video, O_RDWR);
    if (camera->fd < 0)
    {
        printf("can not open %s\n", video);
        goto _err;
    }

    /* 2. query capability */
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(struct v4l2_capability));

    if (0 == ioctl(camera->fd, VIDIOC_QUERYCAP, &cap))
    {        
        if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) 
        {
            fprintf(stderr, "Error opening device %s: video capture not supported.\n", video);
            goto _ioc_querycap_err;
        }

        if(!(cap.capabilities & V4L2_CAP_STREAMING)) 
        {
            fprintf(stderr, "%s does not support streaming i/o\n", video);
            goto _ioc_querycap_err;
        }
    }
    else
    {
        printf("can not get capability\n");
        goto _ioc_querycap_err;
    }

    /* 3. enum formt */
    while (1)
    {
        fmtdesc.index = fmt_index;  
        fmtdesc.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
        if (0 != ioctl(camera->fd, VIDIOC_ENUM_FMT, &fmtdesc))
            break;

        frame_index = 0;
        // printf("format %s,%d:\n", fmtdesc.description, fmtdesc.pixelformat);
        while (1)
        {
            memset(&fsenum, 0, sizeof(struct v4l2_frmsizeenum));
            fsenum.pixel_format = fmtdesc.pixelformat;
            fsenum.index = frame_index;

            /* get framesize */
            if (ioctl(camera->fd, VIDIOC_ENUM_FRAMESIZES, &fsenum) == 0)
            {
                // printf("\t%d: %d x %d\n", frame_index, fsenum.discrete.width, fsenum.discrete.height);
            }
            else
            {
                break;
            }

            frame_index++;
        }

        fmt_index++;
    }

    /* 4. set formt */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(struct v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = camera->frame_x_size;
    fmt.fmt.pix.height = camera->frame_y_size;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (0 == ioctl(camera->fd, VIDIOC_S_FMT, &fmt))
    {
        // printf("the final frame-size has been set : %d x %d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
        camera->frame_x_size = fmt.fmt.pix.width;
        camera->frame_y_size = fmt.fmt.pix.height;
        strncpy(camera->capability, "Motion-JPEG", strlen("Motion-JPEG"));
    }
    else
    {
        printf("can not set format\n");
        goto _ioc_sfmt_err;
    }

    /* 5. require buffer */
    struct v4l2_requestbuffers rb;
    memset(&rb, 0, sizeof(struct v4l2_requestbuffers));
    rb.count = 32;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_MMAP;

    if (0 == ioctl(camera->fd, VIDIOC_REQBUFS, &rb))
    {
        buf_cnt = rb.count;
        for(i = 0; i < rb.count; i++) 
        {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(struct v4l2_buffer));
            buf.index = i;
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (0 == ioctl(camera->fd, VIDIOC_QUERYBUF, &buf))
            {
                /* mmap */
                camera->bufs[i] = mmap(0, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera->fd, buf.m.offset);
                if(camera->bufs[i] == MAP_FAILED) 
                {
                    printf("Unable to map buffer");
                    goto _err;
                }
            }
            else
            {
                printf("can not query buffer\n");
                goto _err;
            }            
        }
    }
    else
    {
        printf("can not request buffers\n");
        goto _ioc_reqbufs_err;
    }

    /* 6. queue buffer */
    for(i = 0; i < buf_cnt; ++i) 
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (0 != ioctl(camera->fd, VIDIOC_QBUF, &buf))
        {
            perror("Unable to queue buffer");
            goto _ioc_qbuf_err;
        }
    }

    camera->bufs_index = 0;     // init camera struct
    camera->buf_length = 0;
    return 0;

_ioc_qbuf_err:
_ioc_reqbufs_err:
_ioc_sfmt_err:
_ioc_querycap_err:
_err:
    return -1;
}

int main(int argc, char **argv)
{
    int ret;
    lcd_mes lcd;
    camera_mes camera;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct pollfd fds[1];

    if (argc != 3)
    {
        printf("Usage: %s </dev/videoX> </dev/fbX>\n", argv[0]);
        return -1;
    }

    /* lcd init */
    ret = lcd_init(argv[2], &lcd);
    if (ret == -1)
    {
        printf("lcd init err !\n");
        goto _err;
    }

    printf("\n-------------- lcd message --------------\n");
    printf("screen pixel: %d x %d\n", lcd.lcd_width, lcd.lcd_height);
    printf("line width: %d (byte)\n", lcd.line_width);
	printf("bpp: %d\n", lcd.bpp);
    printf("-----------------------------------------\n");

    /* camera init */
    camera.frame_x_size = lcd.lcd_width;
    camera.frame_y_size = lcd.lcd_height;
    ret = camera_init(argv[1], &camera);
    if (ret == -1)
    {
        printf("camera init err !\n");
        goto _err;
    }

    printf("\n------------ camera message -------------\n");
    printf("frame size: %d x %d\n", camera.frame_x_size, camera.frame_y_size);
    printf("format: %s\n", camera.capability);
    printf("-----------------------------------------\n");

    /* start camera */
    if (0 != ioctl(camera.fd, VIDIOC_STREAMON, &type))
    {
        printf("Unable to start capture\n");
        goto _err;
    }

    printf("\nstart camera ...\n");
    while (1)
    {
        /* poll */
        memset(fds, 0, sizeof(fds));
        fds[0].fd = camera.fd;
        fds[0].events = POLLIN;
        if (1 == poll(fds, 1, -1))
        {
            /* dequeue buffer */
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(struct v4l2_buffer));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            
            if (0 != ioctl(camera.fd, VIDIOC_DQBUF, &buf))
            {
                printf("Unable to dequeue buffer\n");
                goto _ioc_dqbuf_err;
            }
            
            /* jpeg show on lcd */
            camera.bufs_index = buf.index;
            camera.buf_length = buf.length;
            jpeg_show_on_lcd(&lcd, &camera);

            /* queue buffer */
            if (0 != ioctl(camera.fd, VIDIOC_QBUF, &buf))
            {
                printf("Unable to queue buffer");
                goto _ioc_qbuf_err;
            }
        }
    }

    /* close camera */
    if (0 != ioctl(camera.fd, VIDIOC_STREAMOFF, &type))
    {
        printf("Unable to stop capture\n");
        goto _ioc_streamoff_err;
    }
    close(camera.fd);

    return 0;

_ioc_streamoff_err:
_ioc_qbuf_err:
_ioc_dqbuf_err:
_err:
    return -1;
}


#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/usb.h>
#include<linux/list.h>
#include<linux/mutex.h>
#include<linux/hardirq.h>
#include<linux/bug.h>
#include<linux/poll.h>
#include<linux/spinlock.h>
#include<linux/usb/storage.h>
#include<media/v4l2-dev.h>
#include<media/v4l2-device.h> // used v4l2 registration 
#include<media/v4l2-common.h> // for displaying info
#include<linux/videodev.h>
#include<linux/videodev2.h>
#include<linux/kdev_t.h>
#include<linux/cdev.h>
#include<linux/slab.h>  // Used for kzalloc
#include<linux/uaccess.h>  // get_user and put_user operations
#include<media/v4l2-ioctl.h>
#include<media/videobuf-core.h>
#include <asm/uaccess.h>
#include<linux/fs.h>
#include<linux/mm.h>
#include<asm-generic/ioctl.h>
#include<linux/version.h>
#include<media/videobuf-vmalloc.h>
#include<linux/vmalloc.h>
  
#define MAJOR_NUM 81
#define MAX_BUFFER_SIZE 10
  
// #define IOCTL_SET_MSG _IOR(MAJOR_NUM, 0, char *)                         /*    Macro to write to device */
  
#define IOCTL_GET_MSG _IOR(MAJOR_NUM, 0, char *)
#define IOCTL_GET_NTH_BYTE _IOWR(MAJOR_NUM,0,int)
  
//static unsigned int vid_limit = 16;
  
unsigned long addr_start;
unsigned long addr_end;
int buffer_count=0;
unsigned int mem_size=0;
unsigned int buf_size1=0;
unsigned int buf_count1=0;
struct list_head temp1;
  
size_t buffer_offsets[MAX_BUFFER_SIZE];
  
  
static struct usb_device *device;
static struct video_device *vfd; 
struct cam_fh *fh=NULL;
  
static struct v4l2_queryctrl v4l2_qc;
//static struct v4l2_capability *v4l2_cap;
static struct v4l2_input ip;
static struct v4l2_input v4l2_ip;
static struct v4l2_pix_format v4l2_pix;
static struct video_capability vid_cap;
  
struct v4l2_buffer *temp;
static struct v4l2_format v4l2_fmt;
static struct v4l2_cropcap v4l2_crop;
static struct v4l2_streamparm v4l2_stream;
static struct v4l2_requestbuffers *v4l2_reqbuff;
  
//static int buff_count=0;
  
unsigned int file_flags;
  
//void *mem=NULL;
  
  
/*struct videobuf_mapping *cam_map = {
        .count=4,
        .start=addr_start,
        .end=addr_end,
        .state=2,
          
};
   
struct videobuf_buffer *cam_vid_buff = {
    .map=cam_map,
    .memory=1,
};*/
enum uvc_buffer_state {
    UVC_BUF_STATE_IDLE  = 0,
    UVC_BUF_STATE_QUEUED    = 1,
    UVC_BUF_STATE_ACTIVE    = 2,
    UVC_BUF_STATE_READY = 3,
    UVC_BUF_STATE_DONE  = 4,
    UVC_BUF_STATE_ERROR = 5,
};
  
struct cam_buffer {
  
    unsigned long vma_use_count;
    struct list_head stream;
      
      
    struct videobuf_buffer *vb;
    struct cam_fmt *fmt;
      
      
    /* Touched by interrupt handler. */
    struct v4l2_buffer buf;
    struct list_head queue;
    wait_queue_head_t wait;
    enum uvc_buffer_state state;
};
  
  
#define MAX_BUFFER 32
  
struct uvc_video_queue {
  
    enum v4l2_buf_type type;
  
    void *mem;
    unsigned int flags;
  
    unsigned int count;
    unsigned int buf_size;
    unsigned int buf_used;
    struct cam_buffer buffer[MAX_BUFFER];
    struct mutex mutex; /* protects buffers and mainqueue */
    spinlock_t irqlock; /* protects irqqueue */
  
    struct list_head mainqueue;
    struct list_head irqqueue;
};
  
/*void queue_init(struct uvc_video_queue *qu, enum v4l2_buf_type type)
{
    mutex_init(&qu->mutex);
    spin_lock_init(&qu->irqlock);
    INIT_LIST_HEAD(&qu->mainqueue);
    qu->type = type;
}*/
  
struct uvc_streaming_control {
    __u16 bmHint;
    __u8  bFormatIndex;
    __u8  bFrameIndex;
    __u32 dwFrameInterval;
    __u16 wKeyFrameRate;
    __u16 wPFrameRate;
    __u16 wCompQuality;
    __u16 wCompWindowSize;
    __u16 wDelay;
    __u32 dwMaxVideoFrameSize;
    __u32 dwMaxPayloadTransferSize;
    __u32 dwClockFrequency;
    __u8  bmFramingInfo;
    __u8  bPreferedVersion;
    __u8  bMinVersion;
    __u8  bMaxVersion;
};
  
  
static struct v4l2_queryctrl erfolg_qc[] = {
    {
        .id            = V4L2_CID_AUDIO_VOLUME,
        .name          = "Volume",
        .minimum       = 0,
        .maximum       = 65535,
        .step          = 65535/100,
        .default_value = 65535,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
        .type          = V4L2_CTRL_TYPE_INTEGER,
    }, {
        .id            = V4L2_CID_BRIGHTNESS,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "Brightness",
        .minimum       = 0,
        .maximum       = 255,
        .step          = 1,
        .default_value = 127,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    }, {
        .id            = V4L2_CID_CONTRAST,
        .type          = V4L2_CTRL_TYPE_INTEGER,
        .name          = "Contrast",
        .minimum       = 0,
        .maximum       = 255,
        .step          = 0x1,
        .default_value = 0x10,
        .flags         = V4L2_CTRL_FLAG_SLIDER,
    }, {
        .id            = V4L2_CID_SATURATION,
            .type          = V4L2_CTRL_TYPE_INTEGER,
            .name          = "Saturation",
            .minimum       = 0,
            .maximum       = 255,
            .step          = 0x1,
            .default_value = 127,
            .flags         = V4L2_CTRL_FLAG_SLIDER,
    }, {
        .id            = V4L2_CID_HUE,
            .type          = V4L2_CTRL_TYPE_INTEGER,
            .name          = "Hue",
            .minimum       = -128,
            .maximum       = 127,
            .step          = 0x1,
            .default_value = 0,
            .flags         = V4L2_CTRL_FLAG_SLIDER,
    }
};
  
  
//vivi_dmaqueue
struct cam_dmaq {
  
            struct list_head active;
            struct task_struct *kthread;
            wait_queue_head_t wq;
            int frame;
            int ini_jiffies;
};
  
enum cam_handle_state {
    CAM_HANDLE_PASSIVE  = 0,
    CAM_HANDLE_ACTIVE   = 1,
};
  
//vivi_fh
struct cam_fh {
    struct cam_struct *dev;
    enum cam_handle_state state;
      
};
  
// vivi_dev
  
struct cam_struct {
    struct list_head list;
    struct v4l2_device *v4d_ptr;
    struct video_device *vfd;
    spinlock_t slock;
    struct mutex mutex;
    struct cam_dmaq vidq;
    struct uvc_streaming_control ctrl;
  
    char timestr[13];
    int mv_count;
    int input;
    int qctl_regs[ARRAY_SIZE(erfolg_qc)];
      
    struct cam_buffer *cbuff;         //check out hereee again...
    struct cam_format *fmt1;
    struct cam_frame *frm;
    unsigned int width,height;
    struct uvc_video_queue *queue;
    struct videobuf_queue *vb_vidq;
    enum v4l2_buf_type type;
};
  
struct cam_frame {
    __u8  bFrameIndex;
    __u8  bmCapabilities;
    __u16 wWidth;
    __u16 wHeight;
    __u32 dwMinBitRate;
    __u32 dwMaxBitRate;
    __u32 dwMaxVideoFrameBufferSize;
    __u8  bFrameIntervalType;
    __u32 dwDefaultFrameInterval;
    __u32 *dwFrameInterval;
};
  
  
struct cam_format {
    __u8 type;
    __u8 index;
    __u8 bpp;
    __u8 colorspace;
    __u32 fcc;
    __u32 flags;
  
    char name[32];
  
    unsigned int nframes;
    struct cam_frame *frame;
};
  
  
//vivi_fmt
struct cam_fmt {
         char  *name;
         u32   fourcc;           //v4l2 format id 
         int   depth;
};
  
  
  
/*
 * VMA operations.
 */
static void my_vm_open(struct vm_area_struct *vma)
{
    struct cam_buffer *buffer = vma->vm_private_data;
    buffer->vma_use_count++;
}
  
static void my_vm_close(struct vm_area_struct *vma)
{
    struct cam_buffer *buffer = vma->vm_private_data;
    buffer->vma_use_count--;
}
  
static const struct vm_operations_struct my_vm_ops = {
    .open       = my_vm_open,
    .close      = my_vm_close,
};
  
/*//vivi_buffer
struct cam_buffer {
  
    struct videobuf_buffer vb;
    struct cam_fmt *fmt;
};*/
  
  
/*static struct cam_fmt formats[] = {
         {
                 .name     = "4:2:2, packed, YUYV",
                 .fourcc   = V4L2_PIX_FMT_YUYV,
                 .depth    = 16,
         },
         {
                 .name     = "4:2:2, packed, UYVY",
                 .fourcc   = V4L2_PIX_FMT_UYVY,
                 .depth    = 16,
         },
         {
                 .name     = "RGB565 (LE)",
                 .fourcc   = V4L2_PIX_FMT_RGB565,
                 .depth    = 16,
         },
         {
                 .name     = "RGB565 (BE)",
                 .fourcc   = V4L2_PIX_FMT_RGB565X, 
                 .depth    = 16,
         },
         {
                 .name     = "RGB555 (LE)",
                 .fourcc   = V4L2_PIX_FMT_RGB555,
                 .depth    = 16,
         },
         {
                 .name     = "RGB555 (BE)",
                 .fourcc   = V4L2_PIX_FMT_RGB555X, 
                 .depth    = 16,
         },
 };
*/
  
  
/*static void free_buffer(struct videobuf_queue *vq, struct cam_buffer *buf)
{
  
         //struct cam_fh  *vfh = vq->priv_data;
         //struct cam_struct *dev  =vfh->dev;
   
         //dprintk(dev, 1, "%s, state: %i\n", __func__, buf->vb.state);
   
         if (in_interrupt())
                 BUG();
   
         videobuf_vmalloc_free(buf->vb);
         //dprintk(dev, 1, "free_buffer: freed\n");
         buf->vb->state = VIDEOBUF_NEEDS_INIT;
 }*/
   
#define QUEUE_STREAMING     (1 << 0)
  
int queue_enable(struct uvc_video_queue *queue, int enable)
{
    unsigned int i;
      
  
    mutex_lock(&queue->mutex);
    if (enable) 
    {
        /*if (uvc_queue_streaming(queue)) {
            ret = -EBUSY;
            goto done;
        }*/
          
        queue->flags |= QUEUE_STREAMING;
        //queue->buf_used = 0;
    } 
    else
    {
        //uvc_queue_cancel(queue, 0);
        //INIT_LIST_HEAD(&queue->mainqueue);
  
        for (i = 0; i < queue->count; ++i)
            queue->buffer[i].state =UVC_BUF_STATE_IDLE;
  
        queue->flags &= ~QUEUE_STREAMING;
    }
  
    mutex_unlock(&queue->mutex);
    return 0;
}
  
  
  
   
#define norm_maxw() 1024
#define norm_maxh() 768
  
   
static int my_open(struct file *fp)
{
      
    struct cam_struct *stream;
    struct cam_fh *handle;
    /* Create the device handle. */
    handle = kzalloc(sizeof *handle, GFP_KERNEL);
      
    //memset(&handle,0,sizeof *handle);
  
    stream = video_drvdata(fp);
    stream->type=1;  
      
//  stream->queue=kzalloc(sizeof(struct uvc_video_queue *), GFP_KERNEL);  //,GFP_KERNEL);
  
    stream->queue=kmalloc(sizeof(struct uvc_video_queue), GFP_KERNEL);  //,GFP_KERNEL);
      
    mutex_init(&stream->queue->mutex);
    spin_lock_init(&stream->queue->irqlock);
    INIT_LIST_HEAD(&stream->queue->mainqueue);
    stream->queue->type = stream-> type;
      
      
    if(&stream->queue == NULL)
                printk(KERN_INFO "stream is null");
      
          
    handle->dev = stream;
    handle->state = 0;
    fp->private_data = handle;
      
    return 0;
}
  
/*static ssize_t my_read(struct file *fp,char __user *buff,size_t len,loff_t *off)
{
      
    struct cam_fh *vfh=fp->private_data;
    printk(KERN_INFO "Erfolg : Now in reading module....");
    if(vfh->type==V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return videobuf_read_stream(vfh->vb_vidq,buff,len,off,0,fp->f_flags);
    //printk(KERN_INFO "Erfolg : Addresses : %p,%p,%d",fp,buff,len);
       
    return 0;
}*/ 
  
static long my_ioctl(struct file *fp, unsigned int ioctl_num, unsigned long ioctl_param)
{
    int i;
    struct video_device *vdev = video_devdata(fp);
      
    struct cam_fh *vfh = (struct cam_fh *)fp->private_data;  
    struct cam_struct *stream = vfh->dev;
      
    mutex_init(&stream->queue->mutex);
    spin_lock_init(&stream->queue->irqlock);
    INIT_LIST_HEAD(&stream->queue->mainqueue);
    INIT_LIST_HEAD(&stream->queue->irqqueue);
    stream->queue->type = 1;
  
    switch (ioctl_num) {
  
  
        case VIDIOC_QUERYCTRL:
            {
                //printk(KERN_INFO " Now checking control ");
                if (copy_from_user(&v4l2_qc, (struct v4l2_queryctrl *)ioctl_param, sizeof(v4l2_qc)))
                    return -EACCES;
                //printk(KERN_INFO "Control ID is : %d",(V4L2_CID_BASE - v4l2_qc.id));
                for(i=0;i<ARRAY_SIZE(erfolg_qc);i++)
                {
                    if(v4l2_qc.id == erfolg_qc[i].id)
                    {
                        if (copy_to_user((struct v4l2_queryctrl *)ioctl_param, &erfolg_qc[i], sizeof(struct v4l2_queryctrl)))
                            return -EACCES;
                        else
                            return 0;
                    }
                }
                return -EINVAL;
            }
            break;
  
        case VIDIOC_QUERYCAP:
            {
                  
                //struct cam_fh *vfh=(struct cam_fh  *)fp->private_data;
                  
                struct v4l2_capability *v4l2_cap=(struct v4l2_capability *)ioctl_param;
                  
                printk(KERN_INFO"in querycap....");
                  
                strcpy(v4l2_cap->driver, "erfolg");
                strcpy(v4l2_cap->card, vdev->name);
                  
                v4l2_cap->version =KERNEL_VERSION(2,6,35);
                  
                printk(KERN_INFO "v4l2 buffer type in querycap....temp->type=%d", stream->type);
                  
                if(stream->type==V4L2_BUF_TYPE_VIDEO_CAPTURE)
                {
                        v4l2_cap->capabilities=V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_STREAMING;
                 }
                 else
                 {
                        v4l2_cap->capabilities=V4L2_CAP_VIDEO_OUTPUT|V4L2_CAP_STREAMING;
                 }
                  
                v4l2_cap->reserved[0]=0;
                v4l2_cap->reserved[1]=0;
                v4l2_cap->reserved[2]=0;
                v4l2_cap->reserved[3]=0;
  
                if(copy_to_user((struct v4l2_capability *)ioctl_param, &v4l2_cap, sizeof(v4l2_cap)))
                {
                    printk(KERN_INFO "Erfolg : Device capabilities could not be delivered..... ");
                    return -EFAULT;
                }
                else
                    printk(KERN_INFO "Erfolg : Device capabilities delivered successfully .....");
                  
                return 0;
            }
            break;
  
        case VIDIOC_ENUMINPUT:
            break;
  
        case VIDIOC_G_INPUT: 
            {
                ip.index=1;
                if (copy_to_user((struct v4l2_input *)ioctl_param, &ip, sizeof(ip)))
                {
                    printk(KERN_INFO " Efolg : failed ");
                    return 1;
                }
                else
                {
                    printk(KERN_INFO " Erfolg : success ");
                    return 0;
                }
            }
            break;
        case VIDIOCSPICT: 
            {
                if (copy_from_user(&v4l2_ip, (struct v4l2_input *)ioctl_param, sizeof(struct v4l2_input)))
                {
                    printk(KERN_INFO "Erfolg : Could not set attributes...");
                    return 1; 
                }
                else
                {
                    printk(KERN_INFO "Erfolg : attributes set...");
                    return 0;
                }
            }
            break;
        case VIDIOC_G_CTRL:
            {
                v4l2_pix.pixelformat=V4L2_PIX_FMT_BGR24;
                if (copy_to_user((struct v4l2_pix_format *)ioctl_param, &v4l2_pix, sizeof(v4l2_pix)))
                {
                    printk(KERN_INFO "Erfolg : GCTRL");
  
                }
                else
                {
                    printk(KERN_INFO "Erfolg : GCTRL");
                    return 0;
                }
            }
            break;
        case VIDIOCGCAP:
            {
                vid_cap.type = VID_TYPE_CAPTURE|VID_TYPE_CHROMAKEY|VID_TYPE_SCALES;
                vid_cap.channels = 1;
                vid_cap.maxwidth = 640;
                vid_cap.minwidth = 16;
                vid_cap.maxheight = 480;
                vid_cap.minheight = 16;
                strcpy(vid_cap.name, "ERFOLG_CAMERA_CAPABILITY");
                if(copy_to_user((struct video_capability *)ioctl_param, &vid_cap, sizeof(vid_cap)))
                {
                    printk(KERN_INFO "Erfolg : Device capabilities could not be delivered..... ");
                    return -EFAULT;
                }
                else
                {
                    printk(KERN_INFO "Erfolg : Device capabilities delivered successfully ..... ");
                    printk(KERN_INFO " Now v4l1 !!");
                }
                return 0;
            }
            break;
        case VIDIOC_REQBUFS:
            {
              
                  
                int ret;
                int i;
                unsigned int size1,count;
                void *mem1=NULL;
                int vid_limit=16;
              
                                                  
                v4l2_reqbuff=(struct v4l2_requestbuffers *)ioctl_param;
                  
                stream->queue->type = v4l2_reqbuff->type;
                  
                printk(KERN_INFO "Erfolg : Buffer Count : %d memory :%d  type : %d",v4l2_reqbuff->count,v4l2_reqbuff->memory,v4l2_reqbuff->type); 
                  
                if (v4l2_reqbuff->type != stream->type || v4l2_reqbuff->memory != V4L2_MEMORY_MMAP)
                                    return -EINVAL;     
                              
                mutex_lock(&stream->mutex);
                  
                printk(KERN_INFO "lock on mutex stream...");
                  
                mutex_lock(&stream->queue->mutex);
  
                printk(KERN_INFO "lock on mutex queue...");
                  
                size1 = 640*480*2;
                  
                count=v4l2_reqbuff->count;
                  
                while (size1 * count > vid_limit * 1024 * 1024)
                                 (count)--;
  
                  
                size1 = PAGE_ALIGN(size1);
                  
                printk(KERN_INFO "2");
                  
                  
                printk(KERN_INFO "printing size after page align=%d", size1);
                  
                //mutex_lock(&stream->queue->mutex);
                  
                for(i=v4l2_reqbuff->count;i>0; --i)
                {
                        mem1 = (void *)vmalloc_32(i * size1);
                }
           
                stream->queue->mem=(unsigned int)mem1;
                  
                mem_size=(unsigned int)mem1;
                printk(KERN_INFO "stream->queue->mem=%d",(int)mem1);
                  
                printk(KERN_INFO "going to for loop...mmap-setup...%d",v4l2_reqbuff->count);
                  
                for (i = 0; i < v4l2_reqbuff->count; ++i)
                {
                     //retval++;
                     printk(KERN_INFO "Now in for loop");
                     memset(&stream->queue->buffer[i], 0, sizeof stream->queue->buffer[i]); 
                     //printk(KERN_INFO "memset successful");
                      
                    stream->queue->buffer[i].buf.index = i;
                    stream->queue->buffer[i].buf.m.offset = i * size1;
                    buffer_offsets[i]=stream->queue->buffer[i].buf.m.offset;
                    stream->queue->buffer[i].buf.length = size1;
                    stream->queue->buffer[i].buf.type = stream->queue->type;
                    stream->queue->buffer[i].buf.field = V4L2_FIELD_NONE;
                    stream->queue->buffer[i].buf.memory = V4L2_MEMORY_MMAP;
                    stream->queue->buffer[i].buf.flags = 0;// V4L2_BUF_FLAG_QUEUED;
                    stream->queue->buffer[i].state=UVC_BUF_STATE_IDLE;//UVC_BUF_STATE_QUEUED;
                    init_waitqueue_head(&stream->queue->buffer[i].wait);
                 }
                    printk(KERN_INFO "Now out of for loop ... buffer allocation");
                    stream->queue->mem = mem1;
                    stream->queue->count = v4l2_reqbuff->count;
                    stream->queue->buf_size = size1;
                    buf_size1= size1;
                    buf_count1=v4l2_reqbuff->count;
                    ret = v4l2_reqbuff->count;   
                           printk(KERN_INFO "Escaped");
                             
                           mutex_unlock(&stream->queue->mutex);
                           printk(KERN_INFO "unlock on mutex queue...");
                             
                           mutex_unlock(&stream->mutex);
                           printk(KERN_INFO "unlock on mutex stream...");
                      
                    return 0;
            }
            break;
              
            case VIDIOC_QUERYBUF:
            {
               int ret=0;
               struct cam_buffer *buf;
                 
               buf=kmalloc(sizeof(struct cam_buffer),GFP_KERNEL);
               //temp=kzalloc(sizeof (struct v4l2_buffer *),GFP_KERNEL);       
            //   memset(&temp,0,sizeof (struct v4l2_buffer));
  
               temp=(struct v4l2_buffer *)ioctl_param;
                 
               //memset(&buf,0,sizeof(struct cam_buffer));              
               buf=&stream->queue->buffer[temp->index];
                  
                //struct videobuf_queue *q=fh->vb_vidq;
                if(temp->index >= stream->queue->count)
                {
                            ret=-EINVAL;
                            printk(KERN_INFO "querybuff returns invalid");
                            goto doneq;
                  
                }
                printk(KERN_INFO "In querybuff.......");
                  
                  
                mutex_lock(&stream->queue->mutex);
                  
                printk(KERN_INFO "Mutex locked on queue in querybuff ... temp->index : %d",temp->index);
                  
                memcpy(temp, &buf->buf, sizeof *temp);
  
                            if (buf->vma_use_count)
                                        temp->flags |=V4L2_BUF_FLAG_MAPPED;
  
                            switch (buf->state)
                             {
                                        case UVC_BUF_STATE_ERROR:
                                        case UVC_BUF_STATE_DONE:
                                            temp->flags |= V4L2_BUF_FLAG_DONE;
                                            break;
                                        case UVC_BUF_STATE_QUEUED:
                                        case UVC_BUF_STATE_ACTIVE:
                                        case UVC_BUF_STATE_READY:
                                            temp->flags |= V4L2_BUF_FLAG_QUEUED;
                                            break;
                                        case UVC_BUF_STATE_IDLE:
                                        default:
                                            break;
                              }
            doneq:  
            mutex_unlock(&stream->queue->mutex);
              
            printk(KERN_INFO "stream->queue->mutex unlocked in querybuff");
              
            return ret;
            }
            break;
              
        /*case VIDIOC_QBUF:
            {
          
                printk(KERN_INFO "In queue buff");
                temp=(struct v4l2_buffer *)ioctl_param;
                  
            return videobuf_qbuf(stream->vb_vidq,temp);
            }
            break;*/
        case VIDIOC_QBUF:
        {
                struct cam_buffer *buf;
                unsigned long flags;
                int ret = 0;
                buf=kmalloc(sizeof(struct cam_buffer), GFP_KERNEL);
                printk(KERN_INFO "in Q-buffsss");
                  
                temp=(struct v4l2_buffer *)ioctl_param;
                  
                //stream->queue = kmalloc(sizeof(struct uvc_video_queue ),GFP_KERNEL);
                  
                stream->queue->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                /*  
                if (temp->type != stream->queue->type || temp->memory != V4L2_MEMORY_MMAP) 
                {
                        printk(KERN_INFO "temp and v4l2 types are not same..in QBUFFF...");
                        return -EINVAL;
                }
                */
                mutex_lock(&stream->queue->mutex);
                  
                printk(KERN_INFO "lock on queue mutex..in QBUFFF");
                /*
                if (temp->index >= stream->queue->count) 
                {
                                printk(KERN_INFO "index and qcount error in Qbufff...");
                                goto doner;
                }*/
                  
                buf = &stream->queue->buffer[temp->index];
                  
                INIT_LIST_HEAD(&buf->stream);
                INIT_LIST_HEAD(&buf->queue);
                INIT_LIST_HEAD(&temp1);
                  
                printk(KERN_INFO "QBUF : buf->stream and buf->queue init");
  
                printk(KERN_INFO "assigning BUF== queue index...in QBUFfff...");
                  
                if (buf->state != UVC_BUF_STATE_IDLE ) 
                {
                        printk(KERN_INFO "buffer state in IDLE......");
                        ret = -EINVAL;
                        goto doner;
                }
                  
               if (temp->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) 
              {
                        printk(KERN_INFO "buffer stype is output in QBUFFFF");
                        ret = -EINVAL;
                        goto doner;
              }
                
              spin_lock_irqsave(&stream->queue->irqlock, flags);    //doubt on this callllll.......
                
              printk(KERN_INFO "QBUFF: spinlock_save executeeddd.....");    
                        
              buf->state = UVC_BUF_STATE_QUEUED;
               
            /*  if (v4l2_buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
                        buf->buf.bytesused = 0;
                else
                        buf->buf.bytesused = temp->bytesused;
            */
              
              
                list_add_tail(&buf->stream, &stream->queue->mainqueue);
                  
                printk(KERN_INFO "QBUF : mainqueue addition");
                list_add_tail(&buf->queue, &stream->queue->irqqueue);
                  
                printk(KERN_INFO "QBUF : irqqueue addition");
                  
                spin_unlock_irqrestore(&stream->queue->irqlock, flags);
                  
                printk(KERN_INFO "QBUFF: spinlock_restore executeeddd.....");             
                
                temp1=stream->queue->mainqueue;               
                                
                doner:
                    mutex_unlock(&stream->queue->mutex);
                    printk(KERN_INFO "mutex unlockedd...on QBUFFF");
                return 0;
        }
      
        break;
          
        case VIDIOC_DQBUF:
            {
              
            int ret=0;
            struct cam_buffer *buf = kmalloc(sizeof (struct cam_buffer), GFP_KERNEL);
            temp=(struct v4l2_buffer *)ioctl_param;
                  
                    mutex_lock(&stream->queue->mutex);
                    printk(KERN_INFO "DQBUF:  lock on queue mutex");
      
                            if (list_empty(&stream->queue->mainqueue)) 
                            {
                                  
                                printk(KERN_INFO "DQBUF : list is empty ... assigning new pointer ");
                                stream->queue->mainqueue=temp1;
                                //ret = -EINVAL;
                                //goto donep;
                            }
  
                            buf = list_first_entry(&stream->queue->mainqueue, struct cam_buffer, stream);
                              
                            printk(KERN_INFO "bufferer state in DQBUF--%d  buffer_index: %d", buf->state,buf->buf.index);
      
                            switch (buf->state) 
                            {
                                        case UVC_BUF_STATE_ERROR:
                                            ret = -EIO;
                                        case UVC_BUF_STATE_DONE:
                                            buf->state = UVC_BUF_STATE_IDLE;
                                            break;
  
                                        case UVC_BUF_STATE_IDLE:
                                        case UVC_BUF_STATE_QUEUED:
                                        case UVC_BUF_STATE_ACTIVE:
                                        case UVC_BUF_STATE_READY:
                                        default:
                                            ret = -EINVAL;
                                            goto donep;
                            }
  
                            list_del(&buf->stream);
                            memcpy(temp, &buf->buf, sizeof *temp);
  
                            if (buf->vma_use_count)
                                temp->flags |= V4L2_BUF_FLAG_MAPPED;
  
                            switch (buf->state)
                             {
                                    case UVC_BUF_STATE_ERROR:
                                    case UVC_BUF_STATE_DONE:
                                        temp->flags |= V4L2_BUF_FLAG_DONE;
                                        break;
                                    case UVC_BUF_STATE_QUEUED:
                                    case UVC_BUF_STATE_ACTIVE:
                                    case UVC_BUF_STATE_READY:
                                        temp->flags |= V4L2_BUF_FLAG_QUEUED;
                                        break;
                                    case UVC_BUF_STATE_IDLE:
                                    default:
                                        break;
                            }
                        donep:
                            mutex_unlock(&stream->queue->mutex);
                            printk(KERN_INFO "DQBUF: unlocked mutexxxxx");
                            return ret;
            }
            break;
        case VIDIOCMCAPTURE:
            {
                printk(KERN_INFO "Erfolg : Making image capture ready .....");
                return 0;
            }
            break;
        case VIDIOC_G_FMT:
            {
                v4l2_fmt.type=1;//V4L2_BUF_TYPE_VIDEO_CAPTURE|V4L2_BUF_TYPE_VIDEO_OUTPUT;
                v4l2_fmt.fmt.pix.width=640;
                v4l2_fmt.fmt.pix.height=480;
                v4l2_fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_BGR24;
                v4l2_fmt.fmt.pix.field=V4L2_FIELD_ANY;      // this field might be causing error .....
                v4l2_fmt.fmt.pix.bytesperline=(v4l2_fmt.fmt.pix.width * v4l2_fmt.fmt.pix.height) >> 3;
                v4l2_fmt.fmt.pix.sizeimage=(v4l2_fmt.fmt.pix.bytesperline * v4l2_fmt.fmt.pix.height);
                v4l2_fmt.fmt.pix.colorspace=V4L2_COLORSPACE_JPEG;
                if(copy_to_user((struct v4l2_format *)ioctl_param, &v4l2_fmt, sizeof(v4l2_fmt)))
                {
                    return -EINVAL;
                }
                else
                {
                    printk(KERN_INFO "Erfolg : Format success");
                    return 0;
                }
            }
            break;
        case VIDIOC_S_FMT:
            {
                if(copy_from_user(&v4l2_fmt, (struct v4l2ormat *)ioctl_param, sizeof(v4l2_fmt)))
                {
                    return -EINVAL;
                }
                else
                {
                    printk(KERN_INFO "Erfolg : set format success %d",v4l2_fmt.type);
                    return 0;
                }
            }
            break;
        case VIDIOC_CROPCAP:
            {
                  
                struct v4l2_cropcap *v4l2_crop1=(struct v4l2_cropcap *)ioctl_param;
                  
               if(v4l2_crop1->type != stream->type)
               {
                        printk(KERN_INFO "crop and stream type mismatchhh...try again...");
                        return -EINVAL;
               }
                   
                //v4l2_crop1->type=1;
                v4l2_crop1->bounds.left=0;
                v4l2_crop1->bounds.top=0;
                  
                mutex_lock(&stream->mutex);
                v4l2_crop1->bounds.width = 640;//stream->cur_frame->wWidth;
  
                v4l2_crop1->bounds.height = 480;//stream->cur_frame->wHeight;
                mutex_unlock(&stream->mutex);
                  
                v4l2_crop1->defrect = v4l2_crop1->bounds;
                /*v4l2_crop1->bounds.width=640;
                v4l2_crop1->bounds.height=480;
                v4l2_crop1->defrect.left=0;
                v4l2_crop1->defrect.top=0;
                v4l2_crop1->defrect.width=640;
                v4l2_crop1->defrect.height=480;*/
                  
                v4l2_crop1->pixelaspect.numerator=1;
                v4l2_crop1->pixelaspect.denominator=1;
                  
                if(copy_to_user((struct v4l2_cropcap *)ioctl_param, &v4l2_crop, sizeof(v4l2_crop)))
                {
                    return -EINVAL;
                }
                else
                {
                    printk(KERN_INFO "Erfolg : Crop success");
                    return 0;
                }
            }
            break;
        case VIDIOC_S_CROP:
            {
                if (copy_from_user(&v4l2_crop, (struct v4l2_cropcap *)ioctl_param, sizeof(struct v4l2_crop)))
                {
                    return -EINVAL;
                }
                else
                {
                    printk(KERN_INFO "Erfolg : Crop success %d",v4l2_crop.type);
                    return 0;
                }
            }
            break;
        case VIDIOC_S_PARM:
        {
                struct v4l2_fract tframe;
                struct v4l2_streamparm *parm=(struct v4l2_streamparm *)ioctl_param;     
  
                      
                if(parm->type!=stream->type)
                {
                        printk(KERN_INFO "stream and param types not equal...");
                        return -EINVAL;
                }
                  
                if (parm->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
                        tframe = parm->parm.capture.timeperframe;
                else
                        tframe = parm->parm.output.timeperframe;
                  
                  
                  
  
                if (copy_from_user(&v4l2_stream, (struct v4l2_streamparm *)ioctl_param, sizeof(struct v4l2_streamparm)))
                {
                    return -EINVAL;
                }
                else
                {
                    printk(KERN_INFO "Erfolg : Parm success  %d",v4l2_stream.type);
                      
                }
                if (parm->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
                            parm->parm.capture.timeperframe = tframe;
                else
                            parm->parm.output.timeperframe = tframe;
  
                return 0;               
            }
            break;
          
           case VIDIOC_STREAMON:
           {
                            enum v4l2_buf_type *type1;
                              
                            type1=(enum v4l2_buf_type *)ioctl_param;
                              
                            stream->queue = kmalloc (sizeof(struct uvc_video_queue), GFP_KERNEL);
                              
                            mutex_init(&stream->queue->mutex);
                              
                            printk(KERN_INFO "Erfolg : In stream on  %d",stream->type);
                              
                            if(*type1!=stream->type) // made change here it was capture
                            {
                                    printk(KERN_INFO "Erfolg : type in streamonnnn and is wrong... ");
                                    return -EINVAL;
                            }
                              
                            mutex_lock(&stream->mutex);
                              
                            printk(KERN_INFO "STREAMON: mutex locked on stream");
                              
                            //assuming video is enabled...
                              
                            //val = queue_enable(stream->queue, 1);
                              
                            mutex_lock(&stream->queue->mutex);
                              
                            printk(KERN_INFO "STREAMON: mutex locked on stream->queue");
                              
                            stream->queue->flags |= QUEUE_STREAMING;
                              
                            printk(KERN_INFO "STREAMON: stream->queue flags changed");
                              
                            mutex_unlock(&stream->queue->mutex);
                              
                            printk(KERN_INFO "STREAMON: mutex unlocked on stream->queue");
                              
                            mutex_unlock(&stream->mutex);
                              
                            printk(KERN_INFO "STREAMON: mutex unlocked on stream");
                              
                            return 0;       
           }
           break;
           case VIDIOC_STREAMOFF:
           {
                            enum v4l2_buf_type *type1;
                            stream->queue = kmalloc (sizeof(struct uvc_video_queue), GFP_KERNEL);
                            mutex_init(&stream->queue->mutex);
                              
                            printk(KERN_INFO "Erfolg : In stream off");
                              
                              
                            type1=(enum v4l2_buf_type *)ioctl_param;
                              
                              
                            if(*type1!=stream->type) // made change here it was capture
                            {
                                    printk(KERN_INFO "Erfolg : type in streamofff and is wrong... :");
                                    return -EINVAL;
                            }
                              
                            mutex_lock(&stream->mutex);
                              
                            mutex_lock(&stream->queue->mutex);
                              
                            //val = queue_enable(stream->queue, 0);
                              
                            for(i=0;i<buffer_count;++i)
                            {
                                        stream->queue->buffer[i].state=UVC_BUF_STATE_IDLE;
                            }
                              
                            stream->queue->flags &= ~ QUEUE_STREAMING;
                              
                            mutex_unlock(&stream->queue->mutex);
                              
                            mutex_unlock(&stream->mutex);
                              
                            return 0;       
                              
           }
           break;
        default :
            printk(KERN_INFO "Erfolg : find new ioctl !!! %X",ioctl_num);
            printk(KERN_INFO "ioctl_num : %d",ioctl_num);
            break;
    }
    return 0;
}
  
static unsigned int my_poll(struct file *fp,poll_table *wait)
{
          
        struct cam_fh *vfh=fp->private_data;
        struct cam_struct *stream1 = vfh->dev;
        struct cam_buffer *buf;
        unsigned int mask=0;
          
        printk(KERN_INFO "Erfolg : In Polling module ");
      
          
        stream1->queue = kmalloc(sizeof (struct uvc_video_queue ), GFP_KERNEL);
          
        buf= kmalloc(sizeof(struct cam_buffer), GFP_KERNEL);
          
        mutex_init(&stream1->queue->mutex);
          
        INIT_LIST_HEAD(&stream1->queue->mainqueue);
          
        mutex_lock(&stream1->queue->mutex);
      
        printk(KERN_INFO "POLL : mutex locked on stream->queue");
          
          
        stream1->queue->mainqueue= temp1;
          
        if(list_empty(&stream1->queue->mainqueue))
        {
                    printk(KERN_INFO "POLL : list is empty");
                    mask |= POLLERR;
                    goto donep;
        }         
          
        buf=list_first_entry(&stream1->queue->mainqueue,struct cam_buffer,stream);
          
          
        poll_wait(fp,&buf->wait,wait);
          
        printk(KERN_INFO "POLL : buf->state : %d",buf->state);
          
        buf->state = UVC_BUF_STATE_DONE;
          
        //stream1->queue->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          
        if(buf->state == UVC_BUF_STATE_DONE || buf->state== UVC_BUF_STATE_ERROR)
        {
                if(stream1->queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
                {
                        mask |= POLLIN | POLLRDNORM;
                        printk(KERN_INFO "POLL : buf->state is capture mask : %d",mask);
                }
                else
                {
                        mask |= POLLOUT | POLLWRNORM;
                        printk(KERN_INFO "POLL : buf->state is not capture mask : %d",mask);
                }
        }
          
        donep:
          
                //temp1=temp1->next;
                mutex_unlock(&stream1->queue->mutex);
                printk(KERN_INFO "POLL : stream->queue mutex unlocked......");
                return mask;
 }
  
/*int my_stream(struct file *fp,struct videobuf_queue *q,struct poll_table_struct *pt)
{
        printk(KERN_INFO "Erfolg : Now in streaming mode");
        return 1; 
}
*/
  
int mapper(struct uvc_video_queue *queue, struct vm_area_struct *vma)
{
      
    struct cam_buffer *uninitialized_var(buffer);
    struct page *page;
    unsigned long addr, start, size;
    unsigned int i=0;
    int ret = 0;
      
    buffer= kmalloc(sizeof(struct cam_buffer), GFP_KERNEL);
      
    queue=kmalloc(sizeof (struct uvc_video_queue),GFP_KERNEL);
  
    mutex_init(&queue->mutex);   
      
    start = vma->vm_start;
    size = vma->vm_end - vma->vm_start;
      
    mutex_lock(&queue->mutex);
      
    queue->mem=mem_size;
    queue->buf_size=buf_size1;
    queue->count=buf_count1;
      
    printk(KERN_INFO "In mappper   size=%ld...vm_Start : %lu   buffer count :%d  msize : %d",size,vma->vm_start,buffer_count,(unsigned int)queue->mem);        
      
      
    printk(KERN_INFO "mapper....mutex lockeddd");
      
    for(i=0;i<queue->count;++i)
    {
                buffer=&queue->buffer[i];
                buffer->buf.m.offset=buffer_offsets[i];
                printk(KERN_INFO "mapper: buffer_offset ... %d  vma_pgoff : %lu",buffer->buf.m.offset,vma->vm_pgoff);
                  
                if ((buffer->buf.m.offset >> PAGE_SHIFT) == vma->vm_pgoff)
                {
                                printk(KERN_INFO "mapper: breaking out of for loop ... %d",i);
                                break;
                }
    }
      
    printk(KERN_INFO "mapper....out of for looopppp.....");
      
    /*
    if (i == queue->count || size != queue->buf_size) {
        ret = -EINVAL;
        goto donep;
    }
    */
    vma->vm_flags |= VM_IO;
        printk(KERN_INFO "mapper....VM flag settttt");
  
    addr = (unsigned long)queue->mem + buffer->buf.m.offset;
      
    while (size > 0)
    {
          
        printk(KERN_INFO "mapper...whileee looopppp");
  
        page = vmalloc_to_page((void *)addr);
        if(page==NULL)
        {
            printk(KERN_INFO "mapper....page nullklkll");
            goto donep;
        }
        if ((ret = vm_insert_page(vma, start, page)) < 0)
        {
              
            goto donep;
        }
        start += PAGE_SIZE;
        addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
  
    vma->vm_ops = &my_vm_ops;
    vma->vm_private_data = buffer;
    my_vm_open(vma);
  
donep:
    mutex_unlock(&queue->mutex);
        printk(KERN_INFO "mapper....mutex unnnlockedd");
  
    return ret;
}
   
static int my_map(struct file *fp,struct vm_area_struct *vma)
{
        int ret1;
          
        struct cam_fh *vfh = (struct cam_fh *)fp->private_data;
        struct cam_struct *stream = vfh->dev;
  
        printk(KERN_INFO "in my mapperr...");        
        
        if(vma==NULL)
        {
            printk(KERN_INFO "Erfolg : Vma is null");
            return 0;
        }
        printk(KERN_INFO  "going to mapperr...");        
          
        ret1=mapper(stream->queue,vma);
          
        return ret1;
}
  
/*static int cam_streamon(struct file *fp,void *priv,enum v4l2_buf_type i)
{
            struct cam_fh *vfh=priv;
            if(i!=vfh->type)
                    {
                                printk(KERN_INFO "Erfolg : Streaming type does not match %d",i);
                                return -EINVAL;
                    }
            if(vfh->type!=V4L2_BUF_TYPE_VIDEO_CAPTURE)
                    return -EINVAL;
              
                      
                    return videobuf_streamon(fh->vb_vidq);
  
}
  
static int cam_streamoff(struct file *fp,void *priv,enum v4l2_buf_type i)
{
                printk(KERN_INFO "Erfolg : Now in streamoff");
                return -EINVAL;
  
}
const struct v4l2_ioctl_ops cam_ioctl_ops= {
        .vidioc_streamon=cam_streamon,
        .vidioc_streamoff=cam_streamoff,
};
*/
  
static const struct v4l2_file_operations fops1 = {
      
    .owner=THIS_MODULE,
    //.read=my_read,
    .poll=my_poll,
    .unlocked_ioctl = my_ioctl,
    //.unlocked_ioctl=video_ioctl2,
    .mmap=my_map,
    .open=my_open,
};
  
  
static struct v4l2_device v4l2_d = {
    .name="Erfolg",
};
  
static struct video_device vd = {
    .name  = "Erfolg",
    .fops  = &fops1,
    .minor  = -1,
    .release = video_device_release,
    .tvnorms = V4L2_STD_525_60,
    .current_norm = V4L2_STD_NTSC_M,
    .v4l2_dev=&v4l2_d,
    //.ioctl_ops=&cam_ioctl_ops,
};
  
  
  
  
  
static void cam_disconnect(struct usb_interface *interface)
{
    //struct cam_struct *cam_dev = (struct cam_struct *)video_get_drvdata(vfd); 
  
    //v4l2_device_unregister(cam_dev->v4d_ptr);
    video_device_release(vfd);
    //kfree(cam_dev);
    printk(KERN_INFO "Erfolg :Camera interface no. %d now disconnected\n", interface->cur_altsetting->desc.bInterfaceNumber);
  
    printk(KERN_INFO "Erfolg :Device Port address released.....");
}
   
static struct usb_device_id cam_table[] =
{
    //{USB_DEVICE(0x1bcf,0x2c18)},
    { USB_INTERFACE_INFO(USB_CLASS_VIDEO,USB_SC_8020,USB_PR_CBI) },
    {} /* Terminating entry */
};
  
MODULE_DEVICE_TABLE (usb, cam_table);
  
static int cam_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_host_interface *iface_desc;
    struct cam_struct *cam_dev; 
    int ret;
  
    iface_desc = interface->cur_altsetting; 
  
    // Actual connection establishment
  
    device = interface_to_usbdev(interface);
  
    cam_dev=kzalloc(sizeof(*cam_dev),GFP_KERNEL);
    if (!cam_dev)
    {
        return -ENOMEM;
    }
  
    vfd=video_device_alloc();
    if (!vfd)
    {
        kfree(cam_dev);
        return -ENOMEM;
    }
    *vfd=vd;
    vfd->dev=device->dev;
  
    if ((ret = video_register_device(vfd, 0, -1)) < 0)   // Video Device Registration
    {
        video_device_release(vfd);
        kfree(cam_dev);
        return ret;
    }
  
    video_set_drvdata(vfd,cam_dev);
  
    cam_dev->vfd=vfd;
  
    vfd->dev.init_name="Erfolg";
  
    printk(KERN_INFO "v4l2 device no. :  %d",vfd->num);
  
    if ((ret=v4l2_device_register(&vfd->dev,vfd->v4l2_dev)) < 0)
    {
        printk(KERN_INFO "v4l2 registration : %d",ret); 
        video_device_release(vfd);
        kfree(cam_dev);
        return ret;
    }
  
    cam_dev->v4d_ptr=vfd->v4l2_dev;
  
    v4l2_info(vfd->v4l2_dev,"Erfolg : V4l2 registered as : %d %s %d:%d",vfd->num,vfd->name,MINOR(vfd->dev.devt),MAJOR(vfd->dev.devt));
  
    printk(KERN_INFO "Erfolg : Video device minor no: %d and registration status : %s",vfd->minor,vfd->dev.init_name);
  
    printk(KERN_INFO "Erfolg : Video device registered successfully !! :-)");
  
    printk(KERN_INFO "Erfolg : Camera interface no.  %d now probed: (%04X:%04X)\n",
            iface_desc->desc.bInterfaceNumber, device->descriptor.idVendor,device->descriptor.idProduct);
  
    printk(KERN_INFO "Erfolg : Printing video device pointers major, minor no. and its type  ->%d:%d \t %d"
            ,MAJOR(vfd->dev.devt),MINOR(vfd->dev.devt),vfd->vfl_type);
  
    return 0;
} 
  
static struct usb_driver erfolg =
{
    .name = "erfolg",
    .probe = cam_probe,
    .disconnect = cam_disconnect,
    .id_table = cam_table,
};
   
static int __init imodule(void)
{
    return usb_register(&erfolg);
}
   
static void __exit emodule(void)
{
    usb_deregister(&erfolg);
}
    
module_init(imodule);
module_exit(emodule);
   
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Erfolg Group");
MODULE_DESCRIPTION("USB Video Class Driver");
MODULE_ALIAS("videodev"); 

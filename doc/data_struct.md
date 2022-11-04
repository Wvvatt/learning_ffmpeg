[toc]

## AVFrame

## AVPacket

## AVBufferRef / AVBuffer

```c
struct AVBuffer {
    uint8_t *data;          // 数据实际地址
    size_t size;            // 数据大小
    atomic_uint refcount;   // 引用计数
    void (*free)(void *opaque, uint8_t *data);      // 可以传入释放函数
    void *opaque
    int flags;
    int flags_internal;
};

typedef struct AVBufferRef {
    AVBuffer *buffer;
    uint8_t *data;          // 与buffer->data相同
    size_t   size;          // 与buffer->size相同
} AVBufferRef;
```
这两个结构类似于智能指针的实现，`AVBuffer`是内部结构，`AVBufferRef`是对其的包装。
对于同一个数据，`AVBuffer`只有一个实体，其data指向数据的开始地址。当添加引用时，创建`AVBufferRef`，并使`AVBuffer->refcount`加1。

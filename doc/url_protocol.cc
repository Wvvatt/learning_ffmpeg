#include <iostream>
#include <string>

static void print_split(const std::string &name)
{
    std::cout << "--------------\n";
    std::cout << "---  " << name << "  ---\n";
    std::cout << "--------------\n";
}
/*
URLProtocol这个接口类的实现，实际上可以使用装饰器模式实现
*/
class UrlProtocol
{
public:
    virtual void url_open() = 0;
    virtual void url_read() = 0;
    virtual void url_close() = 0;
};

class UrlContext
{
public:
    void open(){
        prot_->url_open();
    }
    void read() {
        prot_->url_read();
    }
    void close(){
        prot_->url_close();
    }
private:
    UrlProtocol *prot_{nullptr};
    std::string url_;   
};  

class TcpProtocol : public UrlProtocol
{
public:
    void url_open() override {
        std::cout << "tcp open\n";
    }
    void url_read() override {
        std::cout << "tcp read\n";
    }
    void url_close() override{
        std::cout << "tcp close\n";
    }
};
// 协议装饰器
class ProtDecorator : public UrlProtocol
{
public:
    ProtDecorator(UrlProtocol *component) : component_(component){}
    void url_open() override {
        component_->url_open();
    }
    void url_read() override {
        component_->url_read();
    }
    void url_close() override {
        component_->url_close();
    }
private:
    UrlProtocol *component_;    
};  

class RtmpProtocol : public ProtDecorator
{
public:
    RtmpProtocol(UrlProtocol *component) : ProtDecorator(component){};
    void url_open() override {
        ProtDecorator::url_open();
        std::cout << "rtmp open\n";
    }
    void url_read() override {
        ProtDecorator::url_read();
        std::cout << "rtmp read\n";
    }
    void url_close() override{
        ProtDecorator::url_close();
        std::cout << "rtmp close\n";
    }
private:
    std::string name_;      
};

class HttpProtocol : public ProtDecorator
{
public:
    HttpProtocol(UrlProtocol *component) : ProtDecorator(component){};
    void url_open() override {
        ProtDecorator::url_open();
        std::cout << "http open\n";
    }
    void url_read() override {
        ProtDecorator::url_read();
        std::cout << "http read\n";
    }
    void url_close() override{
        ProtDecorator::url_close();
        std::cout << "http close\n";
    }
private:
    std::string name_;      
};

class HlsProtocol : public ProtDecorator
{
public:
    HlsProtocol(UrlProtocol *component) : ProtDecorator(component){};
    void url_open() override {
        ProtDecorator::url_open();
        std::cout << "hls open\n";
    }
    void url_read() override {
        ProtDecorator::url_read();
        std::cout << "hls read\n";
    }
    void url_close() override{
        ProtDecorator::url_close();
        std::cout << "hls close\n";
    }
private:
    std::string name_;      
};


int main()
{   
    TcpProtocol tcp;
    print_split("rtmp");
    RtmpProtocol rtmp(&tcp);
    rtmp.url_open();
    rtmp.url_read();
    rtmp.url_close();

    print_split("http");
    HttpProtocol http(&tcp);
    http.url_open();
    http.url_read();
    http.url_close();

    print_split("hls");
    HlsProtocol hls(&http);
    hls.url_open();
    hls.url_read();
    hls.url_close();
}   
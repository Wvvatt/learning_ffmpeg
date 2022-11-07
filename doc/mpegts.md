[toc]

# MPEGTS

### c++类表示

```cpp
class MpegTsContext
{
public:
    MpegTsFilter *OpenFilter(){
        MpegTsFilter *filter;
        //...
        return filter;
    }
private:
    AVFormatContext *stream_;                           // user data
    std::unorderd_map<int, MpegTsFilter *> pids_;         // filters processing streams specified by PMT and PAT/PMT
}

class MpegTsFilter
{
public:
    
private:
    enum MpegTSFilterType type_;
    MpegTsSectionFilter *section_filter_;
    MpegTsPesFilter *pes_filter_;
};

class MpegTsSectionFilter 
{
public:
    void write_section_data(){
        //...
        section_cb();
        //...
    }
protected:
    virtual void section_cb() = 0;
private:

};

class MpegTsPatFilter : public MpegTsSectionFilter
{
protected:
    void section_cb() override{
        pat_cb();
    }
private:
    void pat_cb(){
        //...
    }
};

class MpegTsPmtFilter : public MpegTsSectionFilter
{
protected:
    void section_cb() override{
        pmt_cb();
    }
private:
    void pmt_cb(){
        //...
    }
};

class MpegTsPesFilter : public MpegTsFilter
{
public:
    void pes_cb(){
        //...
    }
private:
    PESContext *pes_;
};
```
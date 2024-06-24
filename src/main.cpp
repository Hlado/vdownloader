#include "libav/libav.h"

#include <iostream>

int main(int argc, char *argv[])
{
    //auto ctx = libav::Open(R"(https://rr2---sn-n8v7znsz.googlevideo.com/videoplayback?expire=1719177532&ei=3Dx4ZoWqJ6iXv_IPgP-xoAY&ip=83.220.238.64&id=o-ALvHp-MFNmWb8eb5Ebal5SXJ-0Y5-85SlYktpyLNYYev&itag=299&source=youtube&requiressl=yes&xpc=EgVo2aDSNQ%3D%3D&mh=Yl&mm=31%2C26&mn=sn-n8v7znsz%2Csn-4g5edndy&ms=au%2Conr&mv=m&mvi=2&pl=22&initcwndbps=753750&vprv=1&svpuc=1&mime=video%2Fmp4&rqh=1&gir=yes&clen=9384881827&dur=16210.199&lmt=1718060192015288&mt=1719155569&fvip=2&keepalive=yes&c=IOS&txp=7209224&sparams=expire%2Cei%2Cip%2Cid%2Citag%2Csource%2Crequiressl%2Cxpc%2Cvprv%2Csvpuc%2Cmime%2Crqh%2Cgir%2Cclen%2Cdur%2Clmt&sig=AJfQdSswRAIgVaxOO60rQniwzn0psZNKtSghU5ob2KtItHG4fkXUV0UCICwKt8kMkR_Acz0b4JQj8mv9JGF9FeY2-wHOazo-_Mul&lsparams=mh%2Cmm%2Cmn%2Cms%2Cmv%2Cmvi%2Cpl%2Cinitcwndbps&lsig=AHlkHjAwRQIgcROtHKYQoM_Pb1zy71c5LdwMjgERLXmAgCeGhXy-FugCIQDVpuLxSCCHW6Ip9f40cIrdz6P_AXYXtwkbAswYvEwK9w%3D%3D)");
    //libav::Open(ctx,R"(https://rr2---sn-n8v7znsz.googlevideo.com/videoplayback?expire=1719177532&ei=3Dx4ZoWqJ6iXv_IPgP-xoAY&ip=83.220.238.64&id=o-ALvHp-MFNmWb8eb5Ebal5SXJ-0Y5-85SlYktpyLNYYev&itag=299&source=youtube&requiressl=yes&xpc=EgVo2aDSNQ%3D%3D&mh=Yl&mm=31%2C26&mn=sn-n8v7znsz%2Csn-4g5edndy&ms=au%2Conr&mv=m&mvi=2&pl=22&initcwndbps=753750&vprv=1&svpuc=1&mime=video%2Fmp4&rqh=1&gir=yes&clen=9384881827&dur=16210.199&lmt=1718060192015288&mt=1719155569&fvip=2&keepalive=yes&c=IOS&txp=7209224&sparams=expire%2Cei%2Cip%2Cid%2Citag%2Csource%2Crequiressl%2Cxpc%2Cvprv%2Csvpuc%2Cmime%2Crqh%2Cgir%2Cclen%2Cdur%2Clmt&sig=AJfQdSswRAIgVaxOO60rQniwzn0psZNKtSghU5ob2KtItHG4fkXUV0UCICwKt8kMkR_Acz0b4JQj8mv9JGF9FeY2-wHOazo-_Mul&lsparams=mh%2Cmm%2Cmn%2Cms%2Cmv%2Cmvi%2Cpl%2Cinitcwndbps&lsig=AHlkHjAwRQIgcROtHKYQoM_Pb1zy71c5LdwMjgERLXmAgCeGhXy-FugCIQDVpuLxSCCHW6Ip9f40cIrdz6P_AXYXtwkbAswYvEwK9w%3D%3D)");
    
    try {
        libav::AvFormatContext ctx;
        ctx.Open(R"(https://rr2---sn-n8v7znsz.googlevideo.com/videoplayback?expire=1719177532&ei=3Dx4ZoWqJ6iXv_IPgP-xoAY&ip=83.220.238.64&id=o-ALvHp-MFNmWb8eb5Ebal5SXJ-0Y5-85SlYktpyLNYYev&itag=299&source=youtube&requiressl=yes&xpc=EgVo2aDSNQ%3D%3D&mh=Yl&mm=31%2C26&mn=sn-n8v7znsz%2Csn-4g5edndy&ms=au%2Conr&mv=m&mvi=2&pl=22&initcwndbps=753750&vprv=1&svpuc=1&mime=video%2Fmp4&rqh=1&gir=yes&clen=9384881827&dur=16210.199&lmt=1718060192015288&mt=1719155569&fvip=2&keepalive=yes&c=IOS&txp=7209224&sparams=expire%2Cei%2Cip%2Cid%2Citag%2Csource%2Crequiressl%2Cxpc%2Cvprv%2Csvpuc%2Cmime%2Crqh%2Cgir%2Cclen%2Cdur%2Clmt&sig=AJfQdSswRAIgVaxOO60rQniwzn0psZNKtSghU5ob2KtItHG4fkXUV0UCICwKt8kMkR_Acz0b4JQj8mv9JGF9FeY2-wHOazo-_Mul&lsparams=mh%2Cmm%2Cmn%2Cms%2Cmv%2Cmvi%2Cpl%2Cinitcwndbps&lsig=AHlkHjAwRQIgcROtHKYQoM_Pb1zy71c5LdwMjgERLXmAgCeGhXy-FugCIQDVpuLxSCCHW6Ip9f40cIrdz6P_AXYXtwkbAswYvEwK9w%3D%3D)");
        libav::AvFormatContext ctx2{std::move(ctx)};
        std::cout << "All good!" << std::endl;
        return 0;
    } catch(const std::exception &e) {
        std::cout << "There is an error: " << e.what() << std::endl;
    } catch(...) {
        std::cout << "There is some unknown error" << std::endl;
    }

    return -1;
}
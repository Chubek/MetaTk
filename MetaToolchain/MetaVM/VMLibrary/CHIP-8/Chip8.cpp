#include "Chip8.hpp"
#include <algorithm>
#include <stdexcept>
namespace metavm { namespace machines {
Chip8::Quirks Chip8::preset(Profile profile) {
    Quirks q;
    switch(profile) {
    case Profile::legacy:break;
    case Profile::cosmac_vip:
        q.shift_uses_y=true;q.increment_i=true;q.clip_sprites=true;
        q.logic_resets_vf=true;q.wait_for_vblank=true;q.key_wait=KeyWait::release;break;
    case Profile::chip48:
        q.jump_uses_x=true;q.clip_sprites=true;q.increment_i_by_x=true;break;
    default:throw std::invalid_argument("unknown CHIP-8 profile");
    }
    return q;
}
Chip8::Chip8(Profile profile):Chip8(){quirks=preset(profile);}
void Chip8::cancel_wait() {
    drawing_=false;draw_ready_=false;key_waiting_=false;selected_key_=-1;
}
void Chip8::tick_60hz() {
    if(delay)--delay;
    if(sound)--sound;
    if(drawing_)draw_ready_=true;
}
Chip8::Chip8() {
    static const uint8_t font[]={
        0xf0,0x90,0x90,0x90,0xf0,0x20,0x60,0x20,0x20,0x70,0xf0,0x10,0xf0,0x80,0xf0,
        0xf0,0x10,0xf0,0x10,0xf0,0x90,0x90,0xf0,0x10,0x10,0xf0,0x80,0xf0,0x10,0xf0,
        0xf0,0x80,0xf0,0x90,0xf0,0xf0,0x10,0x20,0x40,0x40,0xf0,0x90,0xf0,0x90,0xf0,
        0xf0,0x90,0xf0,0x10,0xf0,0xf0,0x90,0xf0,0x90,0x90,0xe0,0x90,0xe0,0x90,0xe0,
        0xf0,0x80,0x80,0x80,0xf0,0xe0,0x90,0x90,0x90,0xe0,0xf0,0x80,0xf0,0x80,0xf0,
        0xf0,0x80,0xf0,0x80,0x80};
    std::copy(std::begin(font),std::end(font),memory.begin()+0x50);
}
bool Chip8::load(const std::vector<uint8_t>&rom) {
    if(rom.size()>4096-0x200) return false;
    Quirks q=quirks; *this=Chip8(); quirks=q;
    std::copy(rom.begin(),rom.end(),memory.begin()+0x200); return true;
}
Chip8::Status Chip8::step() {
    using S=Status;
    if(pc>4094) return S::bounds;
    unsigned op=(memory[pc]<<8)|memory[pc+1], x=(op>>8)&15,y=(op>>4)&15,n=op&15,nn=op&255,nnn=op&4095;
    // Public state may be changed by the host, including code while blocked.
    if(pc!=blocked_pc_||op!=blocked_opcode_||quirks.key_wait!=blocked_mode_)cancel_wait();
    blocked_pc_=pc;blocked_opcode_=uint16_t(op);blocked_mode_=quirks.key_wait;
    uint16_t next=pc+2; unsigned a=v[x],b=v[y];
    switch(op>>12) {
    case 0: if(op==0x00e0) display.fill(0);
        else if(op==0x00ee) { if(!sp)return S::stack_underflow; if(sp>stack.size())return S::bounds; next=stack[--sp]; }
        else return S::invalid_opcode;
        break;
    case 1: next=nnn; break;
    case 2: if(sp>=16) return S::stack_overflow; stack[sp++]=next; next=nnn; break;
    case 3: if(a==nn) next+=2; break;
    case 4: if(a!=nn) next+=2; break;
    case 5: if(n) return S::invalid_opcode; if(a==b)next+=2; break;
    case 6: v[x]=nn; break;
    case 7: v[x]=a+nn; break;
    case 8:
        switch(n) {
        case 0:v[x]=b;break;
        case 1:case 2:case 3:
            v[x]=n==1?(a|b):n==2?(a&b):(a^b);
            if(quirks.logic_resets_vf)v[15]=0;
            break;
        case 4:v[x]=a+b;v[15]=(a+b)>255;break;
        case 5:v[x]=a-b;v[15]=a>=b;break;
        case 6:b=quirks.shift_uses_y?b:a;v[x]=b>>1;v[15]=b&1;break;
        case 7:v[x]=b-a;v[15]=b>=a;break;
        case 14:b=quirks.shift_uses_y?b:a;v[x]=b<<1;v[15]=(b>>7)&1;break;
        default:return S::invalid_opcode;
        } break;
    case 9:if(n)return S::invalid_opcode;if(a!=b)next+=2;break;
    case 10:i=nnn;break;
    case 11:next=nnn+v[quirks.jump_uses_x?x:0];break;
    case 12:random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;v[x]=random_state&nn;break;
    case 13:
        if(!n)return S::invalid_opcode;
        if(size_t(i)+n>memory.size())return S::bounds;
        if(quirks.wait_for_vblank) {
            if(!drawing_){drawing_=true;return S::waiting_vblank;}
            if(!draw_ready_)return S::waiting_vblank;
        }
        v[15]=0;
        for(unsigned row=0;row<n;++row) {
            for(unsigned bit=0;bit<8;++bit) if(memory[i+row]&(128>>bit)) {
                unsigned px=a%64+bit,py=b%32+row;
                if(quirks.clip_sprites && (px>=64||py>=32))continue;
                auto &pixel=display[(py%32)*64+(px%64)]; v[15]|=pixel;pixel^=1;
            }
        }
        break;
    case 14:
        if(nn!=0x9e && nn!=0xa1)return S::invalid_opcode;
        if(a>=16)return S::bounds;
        if((nn==0x9e && keys[a])||(nn==0xa1 && !keys[a]))next+=2;
        break;
    case 15:
        switch(nn) {
        case 7:v[x]=delay;break;
        case 10:{
            if(quirks.key_wait==KeyWait::held) {
                unsigned k=0;while(k<16&&!keys[k])++k;
                if(k==16)return S::waiting;
                v[x]=uint8_t(k);
            } else if(quirks.key_wait==KeyWait::press) {
                if(!key_waiting_){key_waiting_=true;previous_keys_=keys;return S::waiting;}
                unsigned k=0;while(k<16&&(!keys[k]||previous_keys_[k]))++k;
                previous_keys_=keys;
                if(k==16)return S::waiting;
                v[x]=uint8_t(k);
            } else if(quirks.key_wait==KeyWait::release) {
                if(selected_key_<0) {
                    unsigned k=0;while(k<16&&!keys[k])++k;
                    if(k<16)selected_key_=int(k);
                    return S::waiting;
                }
                if(keys[size_t(selected_key_)])return S::waiting;
                v[x]=uint8_t(selected_key_);
            } else return S::invalid_opcode;
            break;
        }
        case 21:delay=a;break;case 24:sound=a;break;case 30:i=(i+a)&4095;break;case 41:i=0x50+5*(a&15);break;
        case 51:if(size_t(i)+3>4096)return S::bounds;memory[i]=a/100;memory[i+1]=(a/10)%10;memory[i+2]=a%10;break;
        case 85:case 101:
            if(size_t(i)+x+1>4096)return S::bounds;
            for(unsigned k=0;k<=x;++k) if(nn==85)memory[i+k]=v[k];else v[k]=memory[i+k];
            if(quirks.increment_i_by_x)i=(i+x)&4095;
            else if(quirks.increment_i)i=(i+x+1)&4095;
            break;
        default:return S::invalid_opcode;
        } break;
    default:return S::invalid_opcode;
    }
    pc=next;cancel_wait();return S::running;
}
}}

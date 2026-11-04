#pragma once
#include <array>
#include <cmath>

namespace bgdice {
struct V3 {float x,y,z;};
constexpr float Phi=1.6180339887f;
constexpr V3 IcoV[12]={{-1,Phi,0},{1,Phi,0},{-1,-Phi,0},{1,-Phi,0},{0,-1,Phi},{0,1,Phi},{0,-1,-Phi},{0,1,-Phi},
    {Phi,0,-1},{Phi,0,1},{-Phi,0,-1},{-Phi,0,1}};
constexpr int IcoF[20][3]={{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
    {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
inline V3 Centre(int face) {
    auto a=IcoV[IcoF[face][0]],b=IcoV[IcoF[face][1]],c=IcoV[IcoF[face][2]];
    return {(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(a.z+b.z+c.z)/3};
}
inline V3 Rotate(V3 v,float a,float b) {
    auto ca=std::cos(a),sa=std::sin(a),cb=std::cos(b),sb=std::sin(b);
    V3 r{v.x*ca+v.z*sa,v.y,-v.x*sa+v.z*ca};
    return {r.x,r.y*cb-r.z*sb,r.y*sb+r.z*cb};
}
inline const std::array<int,20>& Labels() {
    static auto labels=[]() {
        std::array<int,20> result{};int next=1;
        for(int i=0;i<20;++i) if(!result[i]) {
            auto a=Centre(i);result[i]=next;
            for(int j=i+1;j<20;++j) {
                auto b=Centre(j);
                if(std::abs(a.x+b.x)+std::abs(a.y+b.y)+std::abs(a.z+b.z)<0.001f) result[j]=21-next;
            }
            ++next;
        }
        return result;
    }();
    return labels;
}
struct Pose {float yaw,pitch;};
inline Pose Landing(int value) {
    int face=0;for(int i=0;i<20;++i) if(Labels()[i]==value) face=i;
    auto n=Centre(face);auto yaw=std::atan2(-n.x,n.z);n=Rotate(n,yaw,0);
    return {yaw,std::atan2(n.y,n.z)};
}
inline int FrontNumber(Pose pose) {
    int face=0;float depth=-100;
    for(int i=0;i<20;++i) {auto p=Rotate(Centre(i),pose.yaw,pose.pitch);if(p.z>depth){face=i;depth=p.z;}}
    return Labels()[face];
}
}

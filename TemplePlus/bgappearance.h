#pragma once
// Use the existing Fine Chainmail material AND addmesh bank. No prototype,
// armor statistic, saved object or external game asset is rewritten.
inline int BgChainmailAppearance(int material,bool enabled,int proto=0) {
    // Three locally named woven-chain variants use shared banded-art banks.
    // Match their identities instead of changing every item in those banks.
    if (enabled && (((proto==6296 || proto==6341) && material==20900) ||
        (proto==6301 && material==7600))) return 18400;
    if (enabled && (material==2100 || material==17500 || material==18400 || material==18600 ||
        material==7100 || material==9300 || material==9400 || material==9500 || material==16300 || material==22100)) return 18400;
    return material;
}

// GPU-only source wall masks do not set the native fog occluder bits (0xB0).
// A controlled character may still have an occluded pass; depth-Greater limits
// the silhouette to masked pixels. Keep the ordinary visibility bit required.
inline bool BgOccludedVisible(unsigned fog,bool controlledSourceActor) {
    return (fog&1)!=0 && (controlledSourceActor || (fog&0xB0)!=0);
}
inline bool BgOccludedPass(bool controlledSourceActor,bool partyPrepass) {
    return controlledSourceActor==partyPrepass;
}

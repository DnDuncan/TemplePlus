#pragma once
// Use the existing Fine Chainmail material AND addmesh bank. No prototype,
// armor statistic, saved object or external game asset is rewritten.
inline int BgChainmailAppearance(int material,bool enabled) {
    if (enabled && (material==2100 || material==17500 || material==18400 || material==18600)) return 18400;
    return material;
}

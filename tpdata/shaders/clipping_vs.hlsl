
float4x4 projMat : register(c0);
float objRotCos : register(c4);
float objRotSin : register(c5);
float3 objPos : register(c6);
float3 objScale : register(c7);

float4 main( float3 posIn : POSITION ) : SV_POSITION
{
	float4 pos = float4(posIn, 1);
	/*
		This performs two rotations. First it rotates
		using the obj specific rotation, then it applies
		the game's perspective.
	*/
	float rot1 = (-(objRotCos * pos.y) - -(objRotSin * pos.x));
	float rot2 = - (objRotSin * pos.y + objRotCos * pos.x);
	float rot3 = (rot2 + rot1) * objScale.y * 0.70710599 * 0.70710599;
	float rot4 = (rot1 - rot2) * objScale.x * 0.70710599 * 0.70710599;
	pos.x = objPos.x + rot3 - rot4;
	pos.y = objPos.y + pos.z * objScale.z;
	pos.z = objPos.z + rot4 + rot3;
	float4 projected=mul(projMat,pos);
#ifdef BG_ALWAYS_COVER
	// Source polygon is a foreground mask, independent of actor foot depth.
	projected.z=0.00001f*projected.w;
#endif
	return projected;
}

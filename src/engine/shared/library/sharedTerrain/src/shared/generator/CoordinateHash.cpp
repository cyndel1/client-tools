//===================================================================
//
// CoordinateHash.cpp
// bearhart 6-30-2005
//
// copyright 2005, Sony Online Entertainment
//
//===================================================================

#include "sharedTerrain/FirstSharedTerrain.h"
#include "sharedTerrain/CoordinateHash.h"
#include "sharedRandom/FastRandomGenerator.h"

//===================================================================

namespace
{
   /*
	unsigned int shuffler1[256] = {
	0xd987a3ee,0xcc2ab970,0xde1371a6,0xf8fa5333,0x43be3997,0x7b9d4893,0x8bbc3f47,0x1bab3ec6,
	0x4e6fd45b,0xb35efe80,0x7e9549eb,0x416276de,0x016eded4,0x31a16f30,0x5be25d3a,0x149f0a1e,
	0x0c4f2646,0xe726ccb6,0x7da990d6,0x974250f6,0x2958e3bc,0xf3b220c7,0x320228cf,0xfc37e8bf,
	0x0dc2db52,0x375bbd88,0xbea058e1,0xc7784c45,0xa80f930c,0xb9bb45a7,0x3deff0af,0x723bdced,
	0x5c80c2f8,0xf04769b4,0x774ebc0a,0x189a1e29,0x7c668dd0,0x0e84fc98,0xa9350bc2,0xd2e89c65,
	0xbf3c5156,0xadcc464d,0x19eea891,0x1185793b,0x38d1b60d,0x274ca734,0x92e5c1d1,0x1dd9b1fb,
	0xc2d44fa5,0x8f760df2,0x95ae6eb8,0xd494050b,0x479edd2a,0x6138dae4,0x4228e1e0,0x2c7acf41,
	0x4b0e7a0f,0x732b425c,0xab11c696,0x6ae6ce7f,0xc3fe2254,0x5ff36c7a,0x8d747443,0xf432f2a1,
	0x808aeddd,0x0491a53d,0xb01b568e,0x66c5e758,0x487d3357,0x53e46b07,0x8a6c43da,0xa20a090e,
	0x5ef0ebc0,0x75b994cc,0x7fdf13f4,0xdf6df3a8,0x6420c0ff,0xcecb1812,0xaad3812f,0x54b1198f,
	0xc471e017,0x3cecb553,0x2677c535,0xb460b089,0x511c3690,0x506a78c1,0x1a721a06,0x78962392,
	0x585fa68b,0x2e599f1a,0x94edca49,0x9b5435b7,0x17ff38e7,0xe6b0c4dc,0x10f20777,0xaecdd387,
	0xd80c113c,0x09db2d3e,0xe5f1ee5e,0x9d680471,0x2050c86f,0x9adad6b5,0x7a3954ec,0xbdd8a061,
	0x575d21ca,0xe30d60b2,0x13c7917e,0x3bb35a66,0xdd0730fd,0x63fb83bd,0xd5ba12cd,0x2b0befac,
	0x0b319e69,0xe024662e,0x6fe91086,0x88922c36,0x798c0f24,0x68567531,0x4d9963d5,0xba2d721c,
	0xfd491cbe,0x242c7c75,0xcf63159d,0xf1d27b08,0xc01ec3f0,0x3f08f18d,0x307b3210,0x1c14d727,
	0xb6d6a994,0xd79cb7d9,0xb18e8a99,0x98f77d83,0xe9e18079,0x2a06ae48,0x55f53450,0xa5e768ba,
	0x56ce779a,0x454661fa,0x764bf659,0xcb5c7fa0,0xf56967ce,0x22b785e9,0xe273086b,0xd6d0f5a9,
	0x23452e5d,0xed18a4fe,0x4f64bf00,0x46e39895,0xbc17a2ef,0x82d5996a,0xdcc1d864,0xe812e238,
	0x059088d7,0x8cb4d501,0x81ac3a05,0xac7fd2d8,0x89c9aadb,0x8e10417d,0x9f7c3b6c,0x49b6954f,
	0x87ddcb1d,0x966b31f9,0x0f44cd73,0xdbbf2a44,0x0a330ce6,0x3e70b8a4,0xb28b17f3,0xef61a151,
	0xd122c782,0xf9013cea,0x909b40b9,0xc9518481,0xb8f9ab42,0xeb3a2914,0x448fe9e3,0x744a926e,
	0x361a8fd2,0xc5dce6e8,0xc8c05b16,0x1e051fb3,0x2882be1b,0x60345f4e,0xd316b27b,0xffa80e6d,
	0xd0fce5f1,0x93008923,0x2d045e19,0x07405755,0x3a57d9d3,0xbb1947a3,0xa7a7fbb1,0x063f2f7c,
	0x86b8259e,0x2167b411,0x9e5327b0,0x5a3d9a5f,0xe4aa6a5a,0x33c44acb,0x034d1b4c,0xf2b57304,
	0x59238609,0x70a46467,0x02a3b325,0xfecfac40,0x65def703,0xcd979be2,0x8509ec26,0x6efd1d28,
	0xe19324f5,0xa3552b13,0x99eabb68,0xa03087a2,0xca5aeaad,0xf6ebff3f,0x164102c9,0xeaf41620,
	0xc12e37bb,0x69adfde5,0x83484e78,0x35211422,0x6c7eaf1f,0xb7655c2c,0xaf0352ab,0x3486ba4b,
	0x52794d9b,0x00af658c,0x1227f9ae,0x4a434439,0x671f709c,0x9ca68b37,0x91520674,0xfb81f49f,
	0x40ca8c02,0xda1d9d2b,0x08a2d0aa,0xc6bd9763,0x39e0d132,0xf7887e62,0x6275fafc,0x5dc63dc5,
	0x71f8dfc8,0xa6c80018,0x6dd755df,0xec8d9676,0xee298ec4,0x4c158272,0x2f3e4bf7,0xa42f6285,
	0xfa36ad60,0x1f98c915,0xa1f6038a,0x25c36d4a,0x1583f8c3,0xb589e421,0x84a50184,0x6b25592d
	};

	unsigned int shuffler2[256] = {
	0x2f2300f3,0xb970f3aa,0xec01de51,0xd8ff6adf,0xb3a7b1ff,0x7ee5829d,0x4f7dbcbe,0x3787f8d7,
	0x28b7a03b,0x36f2eb65,0xaa6669c1,0x962293d9,0xc476842e,0xb2418a6e,0xc8032907,0x80e028fd,
	0x4c9b5572,0x954e19d4,0xf5b58088,0x570ddd6f,0x7804b091,0xcb5a9bde,0x796c39e1,0x38840a2b,
	0x33f1b60a,0x3dd7cd76,0x5851cf8a,0x717550f2,0x2c9f5cca,0xa126105f,0xd9aa1474,0x08c72fc9,
	0x29499e4f,0xf4d08886,0xf0a461f6,0x5b7204cc,0x865edb81,0x7b7cd310,0x4734ad28,0x85809cdc,
	0xce12ac9e,0x18b3b47b,0xe5e932f7,0x6386b7e3,0x5a11a727,0xea1f6500,0xc2443ddb,0x9bb0bb14,
	0x75a1f0e9,0x88cf5a8b,0xd5949d3a,0x17fcc637,0x43d8b938,0xf8e1123e,0x7a3a8b77,0x10c9f92d,
	0xbaac7cda,0x68482755,0x0ebcb3ce,0x1e3b0b7d,0x3eeb09c8,0x916a6416,0x341dc30e,0x8f4631a9,
	0x2a0a078c,0x273ebab3,0xbd2fc005,0xa642b849,0x8ceaffea,0xa91bfbd1,0x1d7e75c3,0x87b8bda0,
	0xb0fe44c6,0x6db926ec,0xeb526f48,0x59165378,0x6e9970b7,0x61b4a647,0xe6dce5c7,0x4e208dd0,
	0x8b6bc8e6,0x5ef735ef,0xa41ad1f8,0xed4b9966,0x6085cb35,0x6768d843,0xa7ed5d5e,0xbb71f75c,
	0x7f59474e,0x3582950b,0x05790508,0xfd7f5e31,0xc098c71d,0xafefe090,0xf6c5ab5b,0x0aaec518,
	0x99f002a4,0xd76e63a2,0xdc402b33,0x56e6bed5,0x0b2b4353,0xcf640db0,0x548321ab,0xe20f7485,
	0x131530fb,0xd4456e19,0x448ac4af,0x3ae3eeb6,0xda812d01,0xf36d73d6,0x66271c11,0x1c5794cb,
	0xf100f567,0x2017f668,0x23ce4996,0xb4bb5606,0x89a66269,0xfeb6bf71,0x425b7fbf,0x4ad43302,
	0xa85666b1,0x7d8b1646,0xbfd9c2b9,0x6f9cc16d,0x726236d8,0xfc89e252,0x16c41d82,0xf93620e2,
	0xdde28589,0x7cbf1fa5,0xa5904bac,0x92af5858,0x1a53aebb,0x9ffbd7cf,0x81ee387e,0x9ef91157,
	0x30917b8e,0x46ad40bd,0x827adaf9,0xe1f4ceba,0xbcbdb261,0xe9f5d07f,0x9cc113fa,0x49614e63,
	0x4b3979c2,0x8a304f70,0x395d4529,0x0d35d404,0x50217ec4,0x6acca921,0xfa633779,0xa210222c,
	0xa0180cae,0x9d73b515,0xca08d50d,0xd10e3f98,0xb6243ca6,0x4d50521e,0x94f85484,0x1192816c,
	0xf7a246ad,0x55d68fb4,0x22f31ac5,0x45ca9809,0xd3775f3d,0x40a03423,0x24334a44,0xde37cce0,
	0xee2c5199,0x5f2991e5,0x2d43f4ee,0x843f7d1a,0x129d0ebc,0xffa50fb8,0x15a3687c,0x981ea464,
	0x009ae962,0x6ce46d80,0x253cafe4,0x74197283,0xac314217,0xe4d2f122,0xe32578f5,0x7654609a,
	0x7009e43f,0x2ec80854,0x9a7b41f1,0x5260ece8,0xcd06fc9c,0x03b1effc,0x0f5cd94b,0xc1b21efe,
	0x26ecea0f,0x0158a345,0x3b28dfed,0x931c2ea3,0x09a959e7,0xfbd5489f,0xb52e3b32,0xad380612,
	0xc513edd2,0x19692a2a,0xbe88fe3c,0xc6c6aaeb,0xef6703dd,0xc38c4d30,0x51db1893,0xd22d7625,
	0x838da5b5,0xe8de2442,0x64df87b2,0xccc3869b,0xdbd3d673,0xe795e6f4,0xf2556724,0x0cdd1713,
	0x69149f95,0xc9cd965a,0xa33dfd1f,0x6bda778f,0x532ad250,0x484a2c5d,0xdff6a2f0,0x779ea81b,
	0xe0d16b6b,0x06a85b8d,0x316fcaa8,0x73979a03,0x2b5ffa4a,0xc7c071d3,0x8d4d3aa1,0x3c323e92,
	0x3f938c36,0x1bfd1b94,0x14ba7a56,0x62ab0134,0x414789cd,0xb84f972f,0xb707e74c,0x6578f2a7,
	0x908ea141,0x07c2927a,0xd065dcc0,0x8e4c8320,0x5c8f4c60,0x21746c6a,0x5d02254d,0x32cbe326,
	0x1f0c8e75,0xae0b2340,0x02fae80c,0xabbee197,0xd6965739,0x97e7901c,0x0405c987,0xb1e81559
	};

	uint32 _hash1(uint32 x)
	{
		const uint32 h1 = shuffler[((x>> 0)+  0)&255];
		const uint32 h2 = shuffler[((x>> 8)+ 64)&255];
		const uint32 h3 = shuffler[((x>>16)+128)&255];
		const uint32 h4 = shuffler[((x>>24)+192)&255];

		return h1 ^ h2 ^ h3 ^ h4;
	}

	uint32 _hash2(uint32 x)
	{
		const uint32 h1 = shuffler[((x>> 0)+ 64)&255];
		const uint32 h2 = shuffler[((x>> 8)+128)&255];
		const uint32 h3 = shuffler[((x>>16)+192)&255];
		const uint32 h4 = shuffler[((x>>24)+  0)&255];

		return (h1 + h2) + (h3 + h4);
	}

	uint32 _hash3(uint32 x)
	{
		const uint32 h1 = shuffler[((x>> 0)+128)&255];
		const uint32 h2 = shuffler[((x>> 8)+192)&255];
		const uint32 h3 = shuffler[((x>>16)+  0)&255];
		const uint32 h4 = shuffler[((x>>24)+ 64)&255];

		return (h1 + h2) * (h3 + h4);
	}

	uint32 _hash4(uint32 x)
	{
		const int i = (x>>24) ^ ((x>>16)&255) ^ ((x>>8)&255) ^ (x&255);
		const uint32 h1 = shuffler[i];
		return h1;
	}

	long _hashSeeds[256] = {
		76243,172321,123433,222149,99551,197089,147647,247591,87803,184669,135497,234803,111493,209563,159811,260387,
		75527,171583,122651,221401,98849,196271,146917,246773,87149,183917,134777,234083,110731,208787,159119,259531,
		74779,170777,121951,220589,98047,195469,146099,245981,86369,183091,133993,233183,109903,208049,158293,258677,
		74101,170063,121189,219823,97301,194723,145433,245177,85639,182387,133187,232357,109199,207331,157457,257953,
		73369,169321,120551,219071,96497,193861,144593,244411,84919,181669,132499,231481,108499,206483,156727,257141,
		72647,168499,119771,218389,95783,193073,143797,243631,84191,180731,131713,230683,107773,205763,155851,256189,
		71917,167627,119027,217489,95093,192347,142973,242797,83417,179947,130927,229937,106921,205031,155171,255469,
		71261,166909,118213,216757,94397,191599,142159,241981,82633,179269,130183,229223,106273,204163,154373,254773,
		67589,162917,114547,212671,90787,187639,138433,238159,79159,175519,126421,225307,102409,200177,150611,250777,
		66947,162209,113749,211789,90023,186883,137659,237287,78401,174637,125669,224527,101741,199411,149893,249853,
		66179,161377,113017,211051,89387,186107,136897,236503,77647,173867,124907,223747,101081,198593,149159,249059,
		65537,160637,112223,210241,88681,185467,136261,235607,76991,173059,124171,222977,100297,197831,148469,248293,
		70571,166151,117503,215899,93601,190807,141461,241313,81967,178561,129419,228511,105491,203363,153589,253879,
		69877,165343,116747,215063,92893,189977,140689,240509,81233,177811,128669,227651,104723,202591,152837,253103,
		69109,164443,115931,214243,92251,189271,139981,239713,80611,176921,127931,226903,104009,201797,152077,252253,
		68351,163729,115279,213467,91493,188483,139273,238897,79861,176237,127247,226099,103231,201011,151391,251473
	};

	inline long _hash(unsigned long x)
	{
		return 
			  ( (_hashSeeds[(x>> 0)&255]<<8) + _hashSeeds[(x>> 8)&255] )
			* ( (_hashSeeds[(x>>16)&255]<<8) + _hashSeeds[(x>>24)&255] )
			;
	}
*/

	uint32 inthash(uint32 key)
	{
      key += (key << 12);
      key ^= (key >> 22);
      key += (key << 4);
      key ^= (key >> 9);
      key += (key << 10);
      key ^= (key >> 2);
      key += (key << 7);
      key ^= (key >> 12);
      return key;
	}

   /*
	uint32 _rotateRight(uint32 x, const int count)
	{
		const int modCount = count&31;
		const unsigned returnValue = (x>>modCount) | (x<<(32-modCount));
		return returnValue;
	}

	uint32 _rotateLeft(uint32 x, const int count)
	{
		const int modCount = count&31;
		const unsigned returnValue = (x<<modCount) | (x>>(32-modCount));
		return returnValue;
	}
   */
};

//===================================================================

unsigned long CoordinateHash::hashTuple(float x, float z)
{
	const uint32 ix = (*(uint32 *)&x);
	const uint32 iz = (*(uint32 *)&z)^0xa5a5a5a5;

	const uint32 hx  = inthash(ix);
	const uint32 hz  = inthash(iz);

	const uint32 h = inthash(hx^hz);

	return h;
}

float CoordinateHash::makeFloat(unsigned long hash)
{
	enum { IEEE_FLOAT_BITS=0x3f800000 };

	const uint32 mask = ~uint32(0xff800000);

	const uint32 maskedHash = (hash + (hash>>9))&mask;
	const uint32 iFloat = IEEE_FLOAT_BITS | maskedHash;
	const float returnValue = (*(float *)&iFloat) - 1.0f;

	return returnValue;
}

#if 0
long CoordinateHash::hashTuple(float x, float z)
{
	const unsigned long ix = (*(unsigned long *)&x)^0x5a5a5a5a;
	const unsigned long iz = (*(unsigned long *)&z)^0xa5a5a5a5;
	const unsigned long hx = _hash1((ix>>8) | (ix<<24));
	const unsigned long hz = _hash1((iz>>16) | (iz<<16));
	const unsigned long hmix1 = hx^hz;
	const unsigned long h =  hmix1;

	/*
	FastRandomGenerator rngx(hx);
	FastRandomGenerator rngz(hz);
	const long rnx = rngx.random();
	const long rnz = rngz.random();
	return rnx^rnz;
	*/

	return h;
}
#endif
//===================================================================

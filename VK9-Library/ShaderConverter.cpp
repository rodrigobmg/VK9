/*
Copyright(c) 2016-2019 Christopher Joseph Dean Schaefer

This software is provided 'as-is', without any express or implied
warranty.In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software.If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include "ShaderConverter.h"
#include "LogManager.h"
#include "BitCast.h"

static uint32_t UsageOffsets[14] =
{
	0, //D3DDECLUSAGE_POSITION 0-1 (1 is for tweening)
	2, //D3DDECLUSAGE_BLENDWEIGHT 2-4
	5, //D3DDECLUSAGE_BLENDINDICES 5-7
	8, //D3DDECLUSAGE_NORMAL 8-9
	10, //D3DDECLUSAGE_PSIZE 10
	11, //D3DDECLUSAGE_TEXCOORD 11-26
	27, //D3DDECLUSAGE_TANGENT 27
	28, //D3DDECLUSAGE_BINORMAL 28
	29, //D3DDECLUSAGE_TESSFACTOR 29
	0, //D3DDECLUSAGE_POSITIONT 0 (same as POSITION)
	30, //D3DDECLUSAGE_COLOR 30-31 
	/*Re-use the last three TEXCOORD slots for Fog, Depth, and Sample because 32 is max.*/
	24, //D3DDECLUSAGE_FOG 
	25, //D3DDECLUSAGE_DEPTH
	26 //D3DDECLUSAGE_SAMPLE
};

bool TypeDescription::operator ==(const TypeDescription &value) const
{
	switch (this->PrimaryType)
	{
	case spv::OpTypeBool:
	case spv::OpTypeInt:
	case spv::OpTypeFloat:
	case spv::OpTypeSampler:
	case spv::OpTypeImage:
	case spv::OpTypeVoid:
		return this->PrimaryType == value.PrimaryType;
	case spv::OpTypeArray:
	case spv::OpTypeVector:
	case spv::OpTypeMatrix:
		return this->PrimaryType == value.PrimaryType && this->SecondaryType == value.SecondaryType && this->ComponentCount == value.ComponentCount;
	case spv::OpTypePointer:
		return this->PrimaryType == value.PrimaryType && this->SecondaryType == value.SecondaryType && this->TernaryType == value.TernaryType && this->ComponentCount == value.ComponentCount && this->StorageClass == value.StorageClass;
	case spv::OpTypeFunction:
		return this->PrimaryType == value.PrimaryType && this->SecondaryType == value.SecondaryType && this->TernaryType == value.TernaryType;
	default:
		Log(warning) << "operator == - Unsupported data type " << this->PrimaryType;
		return false;
		break;
	}
}

bool TypeDescription::operator !=(const TypeDescription &value) const
{
	switch (this->PrimaryType)
	{
	case spv::OpTypeBool:
	case spv::OpTypeInt:
	case spv::OpTypeFloat:
	case spv::OpTypeSampler:
	case spv::OpTypeImage:
	case spv::OpTypeVoid:
		return this->PrimaryType != value.PrimaryType;
	case spv::OpTypeArray:
	case spv::OpTypeVector:
	case spv::OpTypeMatrix:
		return this->PrimaryType != value.PrimaryType || this->SecondaryType != value.SecondaryType || this->ComponentCount != value.ComponentCount;
	case spv::OpTypePointer:
		return this->PrimaryType != value.PrimaryType || this->SecondaryType != value.SecondaryType || this->TernaryType != value.TernaryType || this->ComponentCount != value.ComponentCount || this->StorageClass != value.StorageClass;
	case spv::OpTypeFunction:
		return this->PrimaryType != value.PrimaryType || this->SecondaryType != value.SecondaryType || this->TernaryType != value.TernaryType;
	default:
		Log(warning) << "operator != - Unsupported data type " << this->PrimaryType;
		return false;
		break;
	}
}

bool TypeDescription::operator <(const TypeDescription &value) const
{
	if (this->PrimaryType < value.PrimaryType)
	{
		return true;
	}
	else if (this->PrimaryType == value.PrimaryType)
	{
		if (this->SecondaryType < value.SecondaryType)
		{
			return true;
		}
		else if (this->SecondaryType == value.SecondaryType)
		{
			if (this->TernaryType < value.TernaryType)
			{
				return true;
			}
			else if (this->TernaryType == value.TernaryType)
			{
				if (this->ComponentCount < value.ComponentCount)
				{
					return true;
				}
				else if (this->ComponentCount == value.ComponentCount)
				{
					if (this->StorageClass < value.StorageClass)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

std::ostream& operator<< (std::ostream& os, TypeDescription& typeDescription)
{
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		switch (typeDescription.SecondaryType)
		{
		case spv::OpTypeVector:
			if (typeDescription.TernaryType == spv::OpTypeInt)
			{
				return os << "*uvec" << typeDescription.ComponentCount;
			}
			else
			{
				return os << "*vec" << typeDescription.ComponentCount;
			}
		case spv::OpTypeMatrix:
			if (typeDescription.TernaryType == spv::OpTypeInt)
			{
				return os << "*umat" << typeDescription.ComponentCount;
			}
			else
			{
				return os << "*mat" << typeDescription.ComponentCount;
			}
		case spv::OpTypeFloat:
			return os << "*float";
		case spv::OpTypeInt:
			return os << "*int";
		case spv::OpTypeBool:
			return os << "*bool";
		case spv::OpTypeImage:
			return os << "*image";
		default:
			return os << "*void";
		}
	}
	else
	{
		switch (typeDescription.PrimaryType)
		{
		case spv::OpTypeVector:
			if (typeDescription.SecondaryType == spv::OpTypeInt)
			{
				return os << "uvec" << typeDescription.ComponentCount;
			}
			else
			{
				return os << "vec" << typeDescription.ComponentCount;
			}
		case spv::OpTypeMatrix:
			if (typeDescription.SecondaryType == spv::OpTypeInt)
			{
				return os << "umat" << typeDescription.ComponentCount;
			}
			else
			{
				return os << "mat" << typeDescription.ComponentCount;
			}
		case spv::OpTypeFloat:
			return os << "float";
		case spv::OpTypeInt:
			return os << "int";
		case spv::OpTypeBool:
			return os << "bool";
		case spv::OpTypeImage:
			return os << "image";
		default:
			return os << "void";
		}
	}
}

bool TypeDescriptionComparator::operator()(const TypeDescription& a, const TypeDescription& b) const
{
	return a < b;
}

uint32_t Pack(uint32_t wordCount, spv::Op opcode)
{
	OpcodeDescription opcodeDescription;

	opcodeDescription.WordCount = wordCount;
	opcodeDescription.Opcode = opcode;

	return opcodeDescription.Word;
}

void PutStringInVector(std::string& text, std::vector<uint32_t>& words)
{
	const char* value = text.c_str();
	for (int32_t i = 0; i < (int32_t)text.length(); i += 4)
	{
		uint32_t difference = text.length() - (i);

		switch (difference)
		{
		case 0:
			break;
		case 1:
			words.push_back(PACK('\0', '\0', '\0', value[i]));
			break;
		case 2:
			words.push_back(PACK('\0', '\0', value[i + 1], value[i]));
			break;
		case 3:
			words.push_back(PACK('\0', value[i + 2], value[i + 1], value[i]));
			break;
		default:
			words.push_back(PACK(value[i + 3], value[i + 2], value[i + 1], value[i]));
			break;
		}
	}

	if (text.length() % 4 == 0)
	{
		words.push_back(0); //null terminator if all words have characters.
	}
}

_D3DSHADER_PARAM_REGISTER_TYPE GetRegisterType(uint32_t token)
{
	return (_D3DSHADER_PARAM_REGISTER_TYPE)(((token & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2) | ((token & D3DSP_REGTYPE_MASK) >> D3DSP_REGTYPE_SHIFT));
}

TypeDescription GetPointerComponentType(const TypeDescription& inputType)
{
	TypeDescription outputType = inputType;

	outputType.PrimaryType = inputType.PrimaryType;
	outputType.SecondaryType = inputType.TernaryType;
	outputType.StorageClass = inputType.StorageClass;

	return outputType;
}

TypeDescription GetComponentType(const TypeDescription& inputType)
{
	TypeDescription outputType = inputType;

	outputType.PrimaryType = inputType.SecondaryType;
	outputType.SecondaryType = inputType.TernaryType;
	outputType.StorageClass = inputType.StorageClass;

	return outputType;
}

TypeDescription GetValueType(const TypeDescription& inputType)
{
	TypeDescription outputType = inputType;

	outputType.PrimaryType = inputType.SecondaryType;
	outputType.SecondaryType = inputType.TernaryType;
	outputType.TernaryType = spv::OpTypeVoid;
	outputType.StorageClass = inputType.StorageClass;
	outputType.ComponentCount = inputType.ComponentCount;

	return outputType;
}

const char* GetRegisterTypeName(_D3DSHADER_PARAM_REGISTER_TYPE token)
{
	switch (token)
	{
	case D3DSPR_TEMP:
		return "D3DSPR_TEMP";
	case D3DSPR_INPUT:
		return "D3DSPR_INPUT";
	case D3DSPR_CONST:
		return "D3DSPR_CONST";
	case D3DSPR_TEXTURE:
		return "D3DSPR_TEXTURE";
	case D3DSPR_RASTOUT:
		return "D3DSPR_RASTOUT";
	case D3DSPR_ATTROUT:
		return "D3DSPR_ATTROUT";
	case D3DSPR_OUTPUT:
		return "D3DSPR_OUTPUT";
	case D3DSPR_CONSTINT:
		return "D3DSPR_CONSTINT";
	case D3DSPR_COLOROUT:
		return "D3DSPR_COLOROUT";
	case D3DSPR_DEPTHOUT:
		return "D3DSPR_DEPTHOUT";
	case D3DSPR_SAMPLER:
		return "D3DSPR_SAMPLER";
	case D3DSPR_CONST2:
		return "D3DSPR_CONST2";
	case D3DSPR_CONST3:
		return "D3DSPR_CONST3";
	case D3DSPR_CONST4:
		return "D3DSPR_CONST4";
	case D3DSPR_CONSTBOOL:
		return "D3DSPR_CONSTBOOL";
	case D3DSPR_LOOP:
		return "D3DSPR_LOOP";
	case D3DSPR_TEMPFLOAT16:
		return "D3DSPR_TEMPFLOAT16";
	case D3DSPR_MISCTYPE:
		return "D3DSPR_MISCTYPE";
	case D3DSPR_LABEL:
		return "D3DSPR_LABEL";
	case D3DSPR_PREDICATE:
		return "D3DSPR_PREDICATE";
	default:
		return "";
	}
}

const char* GetRegisterTypeName(uint32_t token)
{
	return GetRegisterTypeName(GetRegisterType(token));
}

const char* GetUsageName(_D3DDECLUSAGE usage)
{
	switch (usage)
	{
	case D3DDECLUSAGE_POSITION:
		return "D3DDECLUSAGE_POSITION";
	case D3DDECLUSAGE_BLENDWEIGHT:
		return "D3DDECLUSAGE_BLENDWEIGHT";
	case D3DDECLUSAGE_BLENDINDICES:
		return "D3DDECLUSAGE_BLENDINDICES";
	case D3DDECLUSAGE_NORMAL:
		return "D3DDECLUSAGE_NORMAL";
	case D3DDECLUSAGE_PSIZE:
		return "D3DDECLUSAGE_PSIZE";
	case D3DDECLUSAGE_TEXCOORD:
		return "D3DDECLUSAGE_TEXCOORD";
	case D3DDECLUSAGE_TANGENT:
		return "D3DDECLUSAGE_TANGENT";
	case D3DDECLUSAGE_BINORMAL:
		return "D3DDECLUSAGE_BINORMAL";
	case D3DDECLUSAGE_TESSFACTOR:
		return "D3DDECLUSAGE_TESSFACTOR";
	case D3DDECLUSAGE_POSITIONT:
		return "D3DDECLUSAGE_POSITIONT";
	case D3DDECLUSAGE_COLOR:
		return "D3DDECLUSAGE_COLOR";
	case D3DDECLUSAGE_FOG:
		return "D3DDECLUSAGE_FOG";
	case D3DDECLUSAGE_DEPTH:
		return "D3DDECLUSAGE_DEPTH";
	case D3DDECLUSAGE_SAMPLE:
		return "D3DDECLUSAGE_SAMPLE";
	default:
		return "";
	}
}

uint32_t GetOpcode(uint32_t token)
{
	return (token & D3DSI_OPCODE_MASK);
}

uint32_t GetOpcodeData(uint32_t token)
{
	return ((token & D3DSP_OPCODESPECIFICCONTROL_MASK) >> D3DSP_OPCODESPECIFICCONTROL_SHIFT);
}

uint32_t GetTextureType(uint32_t token)
{
	return (token & D3DSP_TEXTURETYPE_MASK); // Note this one doesn't shift due to weird D3DSAMPLER_TEXTURE_TYPE enum
}

uint32_t GetRegisterNumber(const Token& token)
{
	uint32_t output = 0;

	output = (token.i & D3DSP_REGNUM_MASK);

	return output;
}

uint32_t GetUsage(uint32_t token)
{
	return token & D3DSP_DCL_USAGE_MASK;
}

uint32_t GetUsageIndex(uint32_t token)
{
	return (token & D3DSP_DCL_USAGEINDEX_MASK) >> D3DSP_DCL_USAGEINDEX_SHIFT;
}

void PrintTokenInformation(const char* tokenName)
{
	Log(info) << tokenName << std::endl;
};

void PrintTokenInformation(const char* tokenName, Token argument1)
{
	Log(info) << tokenName << " - "
		<< argument1.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(argument1.i) << ")" << std::endl;
};

void PrintTokenInformation(const char* tokenName, Token result, Token argument1)
{
	Log(info) << tokenName << " - "
		<< result.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(result.i) << ") "
		<< argument1.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(argument1.i) << ")" << std::endl;
};

void PrintTokenInformation(const char* tokenName, Token result, uint32_t resultId, Token argument1, uint32_t argument1Id)
{
	Log(info) << tokenName << " - "
		<< result.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(result.i) << ") [" << resultId << "] "
		<< argument1.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(argument1.i) << ") [" << argument1Id << "]" << std::endl;
};

void PrintTokenInformation(const char* tokenName, Token result, Token argument1, Token argument2)
{
	Log(info) << tokenName << " - "
		<< result.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(result.i) << ") "
		<< argument1.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(argument1.i) << ") "
		<< argument2.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(argument2.i) << ")" << std::endl;
};

void PrintTokenInformation(const char* tokenName, Token result, Token argument1, Token argument2, Token argument3)
{
	Log(info) << tokenName << " - "
		<< result.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(result.i) << ") "
		<< argument1.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(argument1.i) << ") "
		<< argument2.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(argument2.i) << ") "
		<< argument3.DestinationParameterToken.RegisterNumber << "(" << GetRegisterTypeName(argument3.i) << ")" << std::endl;
};

ShaderConverter::ShaderConverter(vk::Device& device)
	: mDevice(device)
{

}

ShaderConverter::~ShaderConverter()
{

}

Token ShaderConverter::GetNextToken()
{
	Token token;

	mPreviousToken++;
	token.i = *(mNextToken++);
	mSize++;

	return token;
}

void ShaderConverter::SkipTokens(uint32_t numberToSkip)
{
	mNextToken += numberToSkip;
}

uint32_t ShaderConverter::GetNextId()
{
	return mNextId++;
}

void ShaderConverter::SkipIds(uint32_t numberToSkip)
{
	mNextId += numberToSkip;
}

uint32_t ShaderConverter::GetSpirVTypeId(spv::Op registerType, uint32_t id)
{
	TypeDescription description;

	description.PrimaryType = registerType;
	description.StorageClass = spv::StorageClassInput;

	return GetSpirVTypeId(description, id);
}

uint32_t ShaderConverter::GetSpirVTypeId(spv::Op registerType1, spv::Op registerType2)
{
	TypeDescription description;

	description.PrimaryType = registerType1;
	description.SecondaryType = registerType2;
	description.StorageClass = spv::StorageClassInput;

	return GetSpirVTypeId(description);
}

uint32_t ShaderConverter::GetSpirVTypeId(spv::Op registerType1, spv::Op registerType2, uint32_t componentCount)
{
	TypeDescription description;

	description.PrimaryType = registerType1;
	description.SecondaryType = registerType2;
	description.ComponentCount = componentCount;
	description.StorageClass = spv::StorageClassInput;

	return GetSpirVTypeId(description);
}

uint32_t ShaderConverter::GetSpirVTypeId(spv::Op registerType1, spv::Op registerType2, spv::Op registerType3, uint32_t componentCount)
{
	TypeDescription description;

	description.PrimaryType = registerType1;
	description.SecondaryType = registerType2;
	description.TernaryType = registerType3;
	description.ComponentCount = componentCount;
	description.StorageClass = spv::StorageClassInput;

	return GetSpirVTypeId(description);
}

uint32_t ShaderConverter::GetSpirVTypeId(TypeDescription& registerType, uint32_t id)
{
	uint32_t columnTypeId = 0;
	uint32_t pointerTypeId = 0;
	uint32_t arrayTypeId = 0;
	uint32_t returnTypeId = 0;
	uint32_t sampledTypeId = 0;
	uint32_t id2 = 0;

	for (const auto& type : mTypeIdPairs)
	{
		if (type.first == registerType)
		{
#ifdef _EXTRA_SHADER_DEBUG_INFO
			if (type.second == 0)
			{
				throw "";
			}
#endif
			return type.second;
		}
	}

	if (id == UINT_MAX)
	{
		id = GetNextId();
	}

	mTypeIdPairs[registerType] = id;
	mIdTypePairs[id] = registerType;

	switch (registerType.PrimaryType)
	{
	case spv::OpTypeBool:
		mTypeInstructions.push_back(Pack(2, registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id
		break;
	case spv::OpTypeInt:
		mTypeInstructions.push_back(Pack(4, registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id
		mTypeInstructions.push_back(32); //Number of bits.
		mTypeInstructions.push_back(0); //Signedness (0 = unsigned,1 = signed)
		break;
	case spv::OpTypeFloat:
		mTypeInstructions.push_back(Pack(3, registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id
		mTypeInstructions.push_back(32); //Number of bits.
		break;
	case spv::OpTypeMatrix:
		columnTypeId = GetSpirVTypeId(spv::OpTypeVector, spv::OpTypeFloat, registerType.ComponentCount);

		mTypeInstructions.push_back(Pack(4, registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id
		mTypeInstructions.push_back(columnTypeId); //Component/Column Type
		mTypeInstructions.push_back(registerType.ComponentCount);

		mDecorateInstructions.push_back(Pack(3, spv::OpDecorate)); //size,Type
		mDecorateInstructions.push_back(id); //target (Id)
		mDecorateInstructions.push_back(spv::DecorationColMajor); //Decoration Type (Id)	
		//mDecorateInstructions.push_back(spv::DecorationRowMajor); //Decoration Type (Id)	
		break;
	case spv::OpTypeVector:
		columnTypeId = GetSpirVTypeId(registerType.SecondaryType);

		mTypeInstructions.push_back(Pack(4, registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id
		mTypeInstructions.push_back(columnTypeId); //Component/Column Type
		mTypeInstructions.push_back(registerType.ComponentCount);
		break;
	case spv::OpTypeArray:
		arrayTypeId = GetSpirVTypeId(registerType.SecondaryType, registerType.TernaryType);

		mTypeInstructions.push_back(Pack(4, registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id
		mTypeInstructions.push_back(arrayTypeId); // Type
		mTypeInstructions.push_back(mConstantIntegerIds[registerType.ComponentCount]); // Length
		break;
	case spv::OpTypePointer:
		pointerTypeId = GetSpirVTypeId(registerType.SecondaryType, registerType.TernaryType, registerType.ComponentCount);

		mTypeInstructions.push_back(Pack(4, registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id
		if (registerType.SecondaryType == spv::OpTypeSampledImage || registerType.SecondaryType == spv::OpTypeImage || registerType.TernaryType == spv::OpTypeSampledImage || registerType.TernaryType == spv::OpTypeImage)
		{
			mTypeInstructions.push_back(spv::StorageClassUniformConstant); //Storage Class
		}
		else
		{
			mTypeInstructions.push_back(registerType.StorageClass); //Storage Class
		}
		mTypeInstructions.push_back(pointerTypeId); // Type
		break;
	case spv::OpTypeSampler:
		mTypeInstructions.push_back(Pack(2, registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id
		break;
	case spv::OpTypeSampledImage:
	case spv::OpTypeImage:
		id2 = id;
		id = GetNextId();

		sampledTypeId = GetSpirVTypeId(spv::OpTypeFloat);
		mTypeInstructions.push_back(Pack(9, spv::OpTypeImage)); //size,Type
		mTypeInstructions.push_back(id2); //Result (Id)
		mTypeInstructions.push_back(sampledTypeId); //Sampled Type (Id)
		mTypeInstructions.push_back(spv::Dim2D); //dimensionality
		mTypeInstructions.push_back(0); //Depth
		mTypeInstructions.push_back(0); //Arrayed
		mTypeInstructions.push_back(0); //MS
		mTypeInstructions.push_back(1); //Sampled
		mTypeInstructions.push_back(spv::ImageFormatUnknown); //Sampled

		mTypeInstructions.push_back(Pack(3, spv::OpTypeSampledImage)); //size,Type
		mTypeInstructions.push_back(id); //Result (Id)
		mTypeInstructions.push_back(id2); //Type (Id)
		break;
	case spv::OpTypeFunction:
	{
		returnTypeId = GetSpirVTypeId(registerType.SecondaryType);

		mTypeInstructions.push_back(Pack(3 + registerType.Arguments.size(), registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id
		mTypeInstructions.push_back(returnTypeId); //Return Type (Id)

		//Right now there is no comparison on arguments so we are assuming that functions with the same return type are the same. This will need to be expanded later when we're using functions other than the default entry point.
		for (int32_t i = 0; i < (int32_t)registerType.Arguments.size(); i++)
		{
			mTypeInstructions.push_back(registerType.Arguments[i]); //Argument Id
		}
	}
	break;
	case spv::OpTypeVoid:
		mTypeInstructions.push_back(Pack(2, registerType.PrimaryType)); //size,Type
		mTypeInstructions.push_back(id); //Id		
		break;
	default:
		Log(warning) << "GetSpirVTypeId - Unsupported data type " << registerType.PrimaryType << std::endl;
		break;
	}

	mTypeIdPairs[registerType] = id;
	mIdTypePairs[id] = registerType;

#ifdef _EXTRA_SHADER_DEBUG_INFO
	if (id == 0)
	{
		throw "";
	}
#endif

	return id;
}

/*
SPIR-V is SSA so this method will generate a new id with the type of the old one when a new "register" is needed.
To handle this result registers will get a new Id each time. The result Id can be used as an input to other operations so this will work fine.
To make sure each call gets the latest id number the lookups must be updated.
*/
uint32_t ShaderConverter::GetNextVersionId(const Token& token)
{
	uint32_t id = GetNextId();

	SetIdByRegister(token, id);

	return id;
}

uint32_t ShaderConverter::GetIdByRegister(const Token& token, _D3DSHADER_PARAM_REGISTER_TYPE type, _D3DDECLUSAGE usage, uint32_t usageIndex)
{
	D3DSHADER_PARAM_REGISTER_TYPE registerType;
	uint32_t registerNumber = 0;
	TypeDescription description;
	uint32_t id = 0;
	uint32_t typeId = 0;
	std::string registerName;

	if (type == D3DSPR_FORCE_DWORD)
	{
		registerType = GetRegisterType(token.i);
	}
	else
	{
		registerType = type;
	}

	switch (registerType)
	{
	case D3DSPR_CONST2:
		registerNumber = GetRegisterNumber(token) + 2048;
		break;
	case D3DSPR_CONST3:
		registerNumber = GetRegisterNumber(token) + 4096;
		break;
	case D3DSPR_CONST4:
		registerNumber = GetRegisterNumber(token) + 6144;
		break;
	case D3DSPR_TEXTURE:
		registerNumber = GetRegisterNumber(token);

		if (mTextures[registerNumber])
		{
			registerNumber = mTextures[registerNumber];
		}

		break;
	default:
		registerNumber = GetRegisterNumber(token);
		break;
	}

	//If a register has already be declared just return the id.
	std::unordered_map<D3DSHADER_PARAM_REGISTER_TYPE, std::unordered_map<uint32_t, uint32_t> >::iterator it1 = mIdsByRegister.find(registerType);
	if (it1 != mIdsByRegister.end())
	{
		std::unordered_map<uint32_t, uint32_t>::iterator it2 = it1->second.find(registerNumber);
		if (it2 != it1->second.end())
		{
			return it2->second;
		}
	}

	/*
	Registers can be defined simply by using them so anything past this point was used before it was declared.
	We'll be missing some usage information and some other bits so we'll try to piece together what it should be from context. (also guess some stuff)

	The rules are different between ps and vs and even between shader models 1, 2, and 3.
	*/

	switch (registerType)
	{
	case D3DSPR_TEXTURE: //Texture could be texcoord
	case D3DSPR_INPUT:
		id = GetNextId();
		description.PrimaryType = spv::OpTypePointer;
		description.SecondaryType = spv::OpTypeVector;

		//TODO: find a way to tell if this is an integer or float.
		if (usage == D3DDECLUSAGE_COLOR)
		{
			description.TernaryType = spv::OpTypeInt; //Hint says this is color so it should be a single DWORD that Vulkan splits into a uvec4.
		}
		else
		{
			description.TernaryType = spv::OpTypeFloat;
		}
		description.ComponentCount = 4;
		description.StorageClass = spv::StorageClassInput;
		typeId = GetSpirVTypeId(description);

		mIdTypePairs[id] = description;

		PushVariable(typeId, id, spv::StorageClassInput);

		mInputRegisters.push_back(id);

		if (registerType == D3DSPR_TEXTURE)
		{
			GenerateDecoration(registerNumber, id, D3DDECLUSAGE_TEXCOORD, usageIndex, true);
		}
		else
		{
			GenerateDecoration(registerNumber, id, D3DDECLUSAGE_COLOR, usageIndex, true);
		}

		break;
	case D3DSPR_TEMP:
	case D3DSPR_RASTOUT:
	case D3DSPR_ATTROUT:
	case D3DSPR_COLOROUT:
	case D3DSPR_DEPTHOUT:
	case D3DSPR_OUTPUT:
		/*
		D3DSPR_TEMP is included with the outputs because for pixel shaders r0 is the color output.
		So rather than duplicate everything I put some logic here and there to decide if it's an output or a temp.
		*/

		id = GetNextId();
		description.PrimaryType = spv::OpTypePointer;
		description.SecondaryType = spv::OpTypeVector;
		description.TernaryType = spv::OpTypeFloat; //TODO: find a way to tell if this is an integer or float.
		description.ComponentCount = 4;
		if (this->mIsVertexShader && registerType != D3DSPR_TEMP)
		{
			description.StorageClass = spv::StorageClassOutput; //Storage Class
		}
		else if (!this->mIsVertexShader && registerType == D3DSPR_TEMP && registerNumber == 0) //r0 is used for pixel shader color output because reasons.
		{
			description.StorageClass = spv::StorageClassOutput; //Storage Class
		}
		else
		{
			description.StorageClass = spv::StorageClassPrivate; //Storage Class
		}

		typeId = GetSpirVTypeId(description);

		mIdTypePairs[id] = description;

		mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
		mTypeInstructions.push_back(typeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
		mTypeInstructions.push_back(id); //Result (Id)

		if (this->mIsVertexShader && registerType != D3DSPR_TEMP)
		{
			mTypeInstructions.push_back(spv::StorageClassOutput); //Storage Class
		}
		else if (!this->mIsVertexShader && registerType == D3DSPR_TEMP && registerNumber == 0) //r0 is used for pixel shader color output because reasons.
		{
			mTypeInstructions.push_back(spv::StorageClassOutput); //Storage Class
		}
		else
		{
			mTypeInstructions.push_back(spv::StorageClassPrivate); //Storage Class
		}

		/*
		This whole block needs to be evaluated. SM2 appears to be able to use ATTROUT for color and until sm3 output doesn't have to be declared.
		This block works in my test cases but I doubt it fits all cases.
		*/
		if (this->mIsVertexShader)
		{
			switch (mMajorVersion)
			{
			case 1:
				switch (registerType)
				{
				case D3DSPR_RASTOUT:
					usage = D3DDECLUSAGE_POSITION;
					break;
				case D3DSPR_ATTROUT:
				case D3DSPR_COLOROUT:
					usage = D3DDECLUSAGE_COLOR;
					break;
				case D3DSPR_TEXCRDOUT:
					usage = D3DDECLUSAGE_TEXCOORD;
					break;
				default:
					//just leave it a lone.
					break;
				}
				break;
			case 2:
				switch (registerType)
				{
				case D3DSPR_RASTOUT:
					usage = D3DDECLUSAGE_POSITION;
					break;
				case D3DSPR_ATTROUT:
				case D3DSPR_OUTPUT:
				case D3DSPR_COLOROUT:
					usage = D3DDECLUSAGE_COLOR;
					break;
				default:
					//just leave it a lone.
					break;
				}
				break;
			case 3:
				//just leave it a lone.
				break;
			default:
				//just leave it a lone.
				break;
			}
		}
		else
		{
			switch (registerType)
			{
			case D3DSPR_DEPTHOUT:
				usage = D3DDECLUSAGE_DEPTH;
				break;
			default:
				usage = D3DDECLUSAGE_COLOR;
				break;
			}
		}

		//mOutputRegisterUsages[(_D3DDECLUSAGE)GetUsage(token.i)] = id;
		if (this->mIsVertexShader && registerType != D3DSPR_TEMP)
		{
			mOutputRegisters.push_back(id);

			GenerateDecoration(registerNumber, id, usage, usageIndex, false);
		}
		else if (!this->mIsVertexShader && registerType == D3DSPR_TEMP && registerNumber == 0) //r0 is used for pixel shader color output because reasons.
		{
			mOutputRegisters.push_back(id);

			GenerateDecoration(registerNumber, id, usage, usageIndex, false);
		}

		break;
	case D3DSPR_CONSTINT:
	{
		id = GetNextId();

		TypeDescription integerVectorType;
		integerVectorType.PrimaryType = spv::OpTypeVector;
		integerVectorType.SecondaryType = spv::OpTypeInt;
		integerVectorType.ComponentCount = 4;
		uint32_t integerVectorTypeId = GetSpirVTypeId(integerVectorType);

		mIdTypePairs[id] = integerVectorType;

		TypeDescription integerVectorPointerType;
		integerVectorPointerType.PrimaryType = spv::OpTypePointer;
		integerVectorPointerType.SecondaryType = spv::OpTypeVector;
		integerVectorPointerType.TernaryType = spv::OpTypeInt;
		integerVectorPointerType.ComponentCount = 4;
		integerVectorPointerType.StorageClass = spv::StorageClassUniform;
		uint32_t integerVectorPointerTypeId = GetSpirVTypeId(integerVectorPointerType);

		TypeDescription integerType;
		integerType.PrimaryType = spv::OpTypeInt;
		uint32_t integerTypeId = GetSpirVTypeId(integerType);

		uint32_t memberIndexId = GetNextId();
		mIdTypePairs[memberIndexId] = integerType;
		mTypeInstructions.push_back(Pack(3 + 1, spv::OpConstant)); //size,Type
		mTypeInstructions.push_back(integerTypeId); //Result Type (Id)
		mTypeInstructions.push_back(memberIndexId); //Result (Id)
		mTypeInstructions.push_back(registerNumber); //Literal Value

		uint32_t chainId = GetNextId();
		mIdTypePairs[chainId] = integerVectorPointerType;

		PushAccessChain(integerVectorPointerTypeId, chainId, mUboPointerId, memberIndexId);

		registerName = "i" + std::to_string(registerNumber);
		PushName(chainId, registerName);

		mIdTypePairs[id] = integerVectorType;

		PushLoad(integerVectorTypeId, id, chainId);
	}
	break;
	case D3DSPR_CONSTBOOL:
	{
		id = GetNextId();

		TypeDescription integerType;
		integerType.PrimaryType = spv::OpTypeInt;
		uint32_t integerTypeId = GetSpirVTypeId(integerType);

		mIdTypePairs[id] = integerType;

		TypeDescription integerPointerType;
		integerPointerType.PrimaryType = spv::OpTypePointer;
		integerPointerType.SecondaryType = spv::OpTypeInt;
		integerPointerType.StorageClass = spv::StorageClassUniform;
		uint32_t integerPointerTypeId = GetSpirVTypeId(integerPointerType);

		uint32_t memberIndexId = GetNextId();
		mIdTypePairs[memberIndexId] = integerType;
		mTypeInstructions.push_back(Pack(3 + 1, spv::OpConstant)); //size,Type
		mTypeInstructions.push_back(integerTypeId); //Result Type (Id)
		mTypeInstructions.push_back(memberIndexId); //Result (Id)
		mTypeInstructions.push_back(mNumberOfIntegerVectorsInUbo + registerNumber); //Literal Value

		uint32_t chainId = GetNextId();
		mIdTypePairs[chainId] = integerPointerType;

		PushAccessChain(integerPointerTypeId, chainId, mUboPointerId, memberIndexId);

		registerName = "b" + std::to_string(registerNumber);
		PushName(chainId, registerName);

		mIdTypePairs[id] = integerType;

		PushLoad(integerTypeId, id, chainId);
	}
	break;
	case D3DSPR_CONST:
	case D3DSPR_CONST2:
	case D3DSPR_CONST3:
	case D3DSPR_CONST4:
	{
		id = GetNextId();

		TypeDescription floatVectorType;
		floatVectorType.PrimaryType = spv::OpTypeVector;
		floatVectorType.SecondaryType = spv::OpTypeFloat;
		floatVectorType.ComponentCount = 4;
		uint32_t floatVectorTypeId = GetSpirVTypeId(floatVectorType);

		mIdTypePairs[id] = floatVectorType;

		TypeDescription floatVectorPointerType;
		floatVectorPointerType.PrimaryType = spv::OpTypePointer;
		floatVectorPointerType.SecondaryType = spv::OpTypeVector;
		floatVectorPointerType.TernaryType = spv::OpTypeFloat;
		floatVectorPointerType.ComponentCount = 4;
		floatVectorPointerType.StorageClass = spv::StorageClassUniform;
		uint32_t floatVectorPointerTypeId = GetSpirVTypeId(floatVectorPointerType);

		TypeDescription integerType;
		integerType.PrimaryType = spv::OpTypeInt;
		uint32_t integerTypeId = GetSpirVTypeId(integerType);

		uint32_t memberIndexId = GetNextId();
		mIdTypePairs[memberIndexId] = integerType;
		mTypeInstructions.push_back(Pack(3 + 1, spv::OpConstant)); //size,Type
		mTypeInstructions.push_back(integerTypeId); //Result Type (Id)
		mTypeInstructions.push_back(memberIndexId); //Result (Id)
		mTypeInstructions.push_back(mNumberOfIntegerVectorsInUbo + mNumberOfBooleansInUbo + registerNumber); //Literal Value

		uint32_t chainId = GetNextId();
		mIdTypePairs[chainId] = floatVectorPointerType;

		PushAccessChain(floatVectorPointerTypeId, chainId, mUboPointerId, memberIndexId);

		registerName = "c" + std::to_string(registerNumber);
		PushName(chainId, registerName);

		mIdTypePairs[id] = floatVectorType;

		PushLoad(floatVectorTypeId, id, chainId);
	}
	break;
	case D3DSPR_SAMPLER:
		description.PrimaryType = spv::OpTypePointer;
		description.SecondaryType = spv::OpTypeImage;
		description.StorageClass = spv::StorageClassUniformConstant; //spv::StorageClassUniformConstant;
		typeId = GetSpirVTypeId(description);

		id = GetNextId();
		PushAccessChain(typeId, id, mTexturesId, mConstantIntegerIds[registerNumber]);

		mIdTypePairs[id] = description;

		registerName = "s" + std::to_string(registerNumber);
		PushName(id, registerName);

		break;
	default:
		Log(warning) << "GetIdByRegister - Id not found register " << registerNumber << " (" << registerType << ")" << std::endl;
		break;
	}

	if (this->mIsVertexShader && registerType == D3DSPR_INPUT)
	{
		switch (usage)
		{
		case D3DDECLUSAGE_COLOR:
			id = FixColor(id);
			break;
		case D3DDECLUSAGE_POSITION:
			id = FixPosition(id);
			break;
		}
	}

	mIdsByRegister[registerType][registerNumber] = id;
	mRegistersById[registerType][id] = registerNumber;

	mUsages[id] = (_D3DDECLUSAGE)usage;

	if (!id)
	{
		Log(warning) << "ShaderConverter::GetIdByRegister - Id was zero!" << std::endl;
	}

	return id;
}

void ShaderConverter::SetIdByRegister(const Token& token, uint32_t id)
{
	D3DSHADER_PARAM_REGISTER_TYPE registerType = GetRegisterType(token.i);
	uint32_t registerNumber = 0;
	TypeDescription description;
	uint32_t typeId = 0;

	switch (registerType)
	{
	case D3DSPR_CONST2:
		registerNumber = GetRegisterNumber(token) + 2048;
		break;
	case D3DSPR_CONST3:
		registerNumber = GetRegisterNumber(token) + 4096;
		break;
	case D3DSPR_CONST4:
		registerNumber = GetRegisterNumber(token) + 6144;
		break;
	default:
		registerNumber = GetRegisterNumber(token);
		break;
	}

	mIdsByRegister[registerType][registerNumber] = id;
	mRegistersById[registerType][id] = registerNumber;
}

TypeDescription ShaderConverter::GetTypeByRegister(const Token& token, _D3DDECLUSAGE usage)
{
	TypeDescription dataType;

	uint32_t id = GetIdByRegister(token, D3DSPR_FORCE_DWORD, usage);

	std::unordered_map<uint32_t, TypeDescription>::iterator it2 = mIdTypePairs.find(id);
	if (it2 != mIdTypePairs.end())
	{
		dataType = it2->second;
	}
	else
	{
		dataType.PrimaryType = spv::OpTypeVector;
		if (usage == D3DDECLUSAGE_COLOR)
		{
			dataType.SecondaryType = spv::OpTypeInt;
		}
		else
		{
			dataType.SecondaryType = spv::OpTypeFloat;
		}
		dataType.ComponentCount = 4;
	}

	return dataType;
}

/*
This function gets the id for the token and if it's a pointer loads it.
It this checks to see if there is a swizzle and if so shuffle or extract to get the right values out.
The swizzle values for sources are different from the write mask on a result which is why we need a seperate function for each. :-(
*/
uint32_t ShaderConverter::GetSwizzledId(const Token& token, uint32_t lookingFor)
{
	uint32_t originalId = 0;
	uint32_t outputComponentCount = 4;

	if (lookingFor == GIVE_ME_SAMPLER)
	{
		originalId = GetIdByRegister(token, D3DSPR_SAMPLER);
	}
	else if (lookingFor == GIVE_ME_SCALAR)
	{
		originalId = GetIdByRegister(token);
		outputComponentCount = 1;
	}
	else if (lookingFor == GIVE_ME_VECTOR_2)
	{
		if (mMajorVersion >= 3)
		{
			originalId = GetIdByRegister(token, D3DSPR_INPUT, D3DDECLUSAGE_TEXCOORD);
		}
		else
		{
			originalId = GetIdByRegister(token, D3DSPR_TEXTURE, D3DDECLUSAGE_TEXCOORD);
		}
		outputComponentCount = 2;
	}
	else if (lookingFor == GIVE_ME_MATRIX_4X4)
	{
		originalId = GetIdByRegister(token);

		uint32_t matrixId = mVector4Matrix4X4Pairs[originalId];
		if (matrixId)
		{
			originalId = matrixId;
		}
		else
		{
			/*
				In DXBC you can use a float register as a mat4 even though it's really a vec4.
				The way it works is the next 3 registers are combined with the one you requested into a mat4.
				We use composite construct to emulate that dark magic.
				Then we throw the id translation in a map so we don't convert it again if it's used later.
			*/
			Token token1 = token;
			token1.DestinationParameterToken.RegisterNumber += 1;
			uint32_t c1 = GetIdByRegister(token1);

			Token token2 = token;
			token2.DestinationParameterToken.RegisterNumber += 2;
			uint32_t c2 = GetIdByRegister(token2);

			Token token3 = token;
			token3.DestinationParameterToken.RegisterNumber += 3;
			uint32_t c3 = GetIdByRegister(token3);

			TypeDescription matrixType;
			matrixType.PrimaryType = spv::OpTypeMatrix;
			matrixType.SecondaryType = spv::OpTypeVector;
			matrixType.TernaryType = spv::OpTypeFloat;
			matrixType.ComponentCount = 4;
			uint32_t matrixTypeId = GetSpirVTypeId(matrixType);

			uint32_t matrixId = GetNextId();

			mIdTypePairs[matrixId] = matrixType;
			Push(spv::OpCompositeConstruct, matrixTypeId, matrixId, originalId, c1, c2, c3);
			mVector4Matrix4X4Pairs[originalId] = matrixId;

			originalId = matrixId;
		}
	}
	else if (lookingFor == GIVE_ME_MATRIX_3X3)
	{
		originalId = GetIdByRegister(token);

		uint32_t matrixId = mVector4Matrix3X3Pairs[originalId];
		if (matrixId)
		{
			originalId = matrixId;
		}
		else
		{
			uint32_t matrixId = mVector4Matrix4X4Pairs[originalId];
			if (matrixId)
			{
				//Couldn't find mat3 version so create one quick and return that instead.
				matrixId = ConvertMat4ToMat3(originalId);
				mVector4Matrix3X3Pairs[originalId] = matrixId;

				originalId = matrixId;
			}
		}
	}
	else if (lookingFor == GIVE_ME_VECTOR_3)
	{
		originalId = GetIdByRegister(token);

		uint32_t vectorId = mVector4Vector3Pairs[originalId];
		if (vectorId)
		{
			originalId = vectorId;
		}
		else
		{
			//Couldn't find vec3 version so create one quick and return that instead.
			vectorId = ConvertVec4ToVec3(originalId);
			mVector4Vector3Pairs[originalId] = vectorId;

			originalId = vectorId;
		}

		outputComponentCount = 3;
	}
	else
	{
		originalId = GetIdByRegister(token);
	}

	TypeDescription originalType = mIdTypePairs[originalId];
	uint32_t originalTypeId = GetSpirVTypeId(originalType);

	uint32_t loadedId = 0;
	TypeDescription loadedType;
	uint32_t loadedTypeId = 0;

	if (originalType.PrimaryType == spv::OpTypePointer)
	{
		loadedType.PrimaryType = originalType.SecondaryType;
		loadedType.SecondaryType = originalType.TernaryType;
		loadedType.TernaryType = spv::OpTypeVoid;
		loadedType.ComponentCount = originalType.ComponentCount;
		loadedTypeId = GetSpirVTypeId(loadedType);

		loadedId = PushLoad(originalId);
	}
	else
	{
		loadedType = originalType;
		loadedId = originalId;
		loadedTypeId = originalTypeId;
	}

	/*
	Check for modifiers and if found apply them to the interim result.
	*/
	uint32_t modifier = token.i & D3DSP_SRCMOD_MASK;

	switch (modifier)
	{
	case D3DSPSM_NONE:
		//Nothing to do here.
		break;
	case D3DSPSM_NEG:
		if (loadedType.PrimaryType == spv::OpTypeFloat || loadedType.SecondaryType == spv::OpTypeFloat)
		{
			uint32_t negatedId = GetNextId();
			mIdTypePairs[negatedId] = loadedType;
			Push(spv::OpFNegate, loadedTypeId, negatedId, loadedId);
			loadedId = negatedId;
		}
		else
		{
			uint32_t negatedId = GetNextId();
			mIdTypePairs[negatedId] = loadedType;
			Push(spv::OpSNegate, loadedTypeId, negatedId, loadedId);
			loadedId = negatedId;
		}
		break;
	case D3DSPSM_BIAS: //(x-0.5)
		Log(warning) << "ShaderConverter::GetSwizzledId - Unsupported modifier type D3DSPSM_BIAS" << std::endl;
		break;
	case D3DSPSM_BIASNEG: //(-(x-0.5))
		Log(warning) << "ShaderConverter::GetSwizzledId - Unsupported modifier type D3DSPSM_BIASNEG" << std::endl;
		break;
	case D3DSPSM_SIGN: //(2(x-0.5))
		if (loadedType.PrimaryType == spv::OpTypeFloat || loadedType.SecondaryType == spv::OpTypeFloat)
		{
			uint32_t absoluteId = GetNextId();
			mIdTypePairs[absoluteId] = loadedType;
			Push(spv::OpExtInst, loadedTypeId, absoluteId, mGlslExtensionId, GLSLstd450::GLSLstd450FSign, loadedId);
			loadedId = absoluteId;
		}
		else
		{
			uint32_t absoluteId = GetNextId();
			mIdTypePairs[absoluteId] = loadedType;
			Push(spv::OpExtInst, loadedTypeId, absoluteId, mGlslExtensionId, GLSLstd450::GLSLstd450SSign, loadedId);
			loadedId = absoluteId;
		}
		break;
	case D3DSPSM_SIGNNEG: //(-2(x - 0.5))
		if (loadedType.PrimaryType == spv::OpTypeFloat || loadedType.SecondaryType == spv::OpTypeFloat)
		{
			uint32_t absoluteId = GetNextId();
			mIdTypePairs[absoluteId] = loadedType;
			Push(spv::OpExtInst, loadedTypeId, absoluteId, mGlslExtensionId, GLSLstd450::GLSLstd450FSign, loadedId);
			loadedId = absoluteId;

			uint32_t negatedId = GetNextId();
			mIdTypePairs[negatedId] = loadedType;
			Push(spv::OpFNegate, loadedTypeId, negatedId, loadedId);
			loadedId = negatedId;
		}
		else
		{
			uint32_t absoluteId = GetNextId();
			mIdTypePairs[absoluteId] = loadedType;
			Push(spv::OpExtInst, loadedTypeId, absoluteId, mGlslExtensionId, GLSLstd450::GLSLstd450SSign, loadedId);
			loadedId = absoluteId;

			uint32_t negatedId = GetNextId();
			mIdTypePairs[negatedId] = loadedType;
			Push(spv::OpSNegate, loadedTypeId, negatedId, loadedId);
			loadedId = negatedId;
		}
		break;
	case D3DSPSM_COMP: //(1-x)
		if (loadedType.PrimaryType == spv::OpTypeFloat || loadedType.SecondaryType == spv::OpTypeFloat)
		{
			uint32_t compId = GetNextId();
			mIdTypePairs[compId] = loadedType;
			Push(spv::OpFSub, loadedTypeId, compId, m1fId, loadedId);
			loadedId = compId;
		}
		else
		{
			uint32_t compId = GetNextId();
			mIdTypePairs[compId] = loadedType;
			Push(spv::OpISub, loadedTypeId, compId, mConstantIntegerIds[1], loadedId);
			loadedId = compId;
		}
		break;
	case D3DSPSM_X2: //(2x)
		Log(warning) << "ShaderConverter::GetSwizzledId - Unsupported modifier type D3DSPSM_X2" << std::endl;
		break;
	case D3DSPSM_X2NEG: //(-2x)
		Log(warning) << "ShaderConverter::GetSwizzledId - Unsupported modifier type D3DSPSM_X2NEG" << std::endl;
		break;
	case D3DSPSM_DZ: //tex{ld,crd} (.xy/.z), z=0 => .11
		Log(warning) << "ShaderConverter::GetSwizzledId - Unsupported modifier type D3DSPSM_DZ" << std::endl;
		break;
	case D3DSPSM_DW: //tex{ld,crd} (.xy/.w), w=0 => .11
		Log(warning) << "ShaderConverter::GetSwizzledId - Unsupported modifier type D3DSPSM_DW" << std::endl;
		break;
	case D3DSPSM_ABS: //(abs(x))
		if (loadedType.PrimaryType == spv::OpTypeFloat || loadedType.SecondaryType == spv::OpTypeFloat)
		{
			uint32_t absoluteId = GetNextId();
			mIdTypePairs[absoluteId] = loadedType;
			Push(spv::OpExtInst, loadedTypeId, absoluteId, mGlslExtensionId, GLSLstd450::GLSLstd450FAbs, loadedId);
			loadedId = absoluteId;
		}
		else
		{
			uint32_t absoluteId = GetNextId();
			mIdTypePairs[absoluteId] = loadedType;
			Push(spv::OpExtInst, loadedTypeId, absoluteId, mGlslExtensionId, GLSLstd450::GLSLstd450SAbs, loadedId);
			loadedId = absoluteId;
		}
		break;
	case D3DSPSM_ABSNEG: //(-abs(x))
		if (loadedType.PrimaryType == spv::OpTypeFloat || loadedType.SecondaryType == spv::OpTypeFloat)
		{
			uint32_t absoluteId = GetNextId();
			mIdTypePairs[absoluteId] = loadedType;
			Push(spv::OpExtInst, loadedTypeId, absoluteId, mGlslExtensionId, GLSLstd450::GLSLstd450FAbs, loadedId);
			loadedId = absoluteId;

			uint32_t negatedId = GetNextId();
			mIdTypePairs[negatedId] = loadedType;
			Push(spv::OpFNegate, loadedTypeId, negatedId, loadedId);
			loadedId = negatedId;
		}
		else
		{
			uint32_t absoluteId = GetNextId();
			mIdTypePairs[absoluteId] = loadedType;
			Push(spv::OpExtInst, loadedTypeId, absoluteId, mGlslExtensionId, GLSLstd450::GLSLstd450SAbs, loadedId);
			loadedId = absoluteId;

			uint32_t negatedId = GetNextId();
			mIdTypePairs[negatedId] = loadedType;
			Push(spv::OpSNegate, loadedTypeId, negatedId, loadedId);
			loadedId = negatedId;
		}
		break;
	case D3DSPSM_NOT: //!
		Log(warning) << "ShaderConverter::GetSwizzledId - Unsupported modifier type D3DSPSM_NOT" << std::endl;
		break;
	default:
		Log(warning) << "ShaderConverter::GetSwizzledId - Unsupported modifier type " << modifier << std::endl;
		break;
	}

	/*
	Use either shuffle or extract to get the component(s) indicated by the swizzle mask.
	There are some special cases where we need to ignore the mask such as texcoord.
	*/
	uint32_t swizzle = token.i & D3DVS_SWIZZLE_MASK;

	if ((swizzle == 0 || swizzle == D3DVS_NOSWIZZLE || lookingFor == GIVE_ME_SAMPLER) && (lookingFor != GIVE_ME_SCALAR))
	{
		return loadedId; //No swizzle no op.
	}

	if (lookingFor == GIVE_ME_MATRIX_4X4 || lookingFor == GIVE_ME_MATRIX_3X3)
	{
		//TODO: figure out what to do if we need a 3x3 but have a 4x4 or the other way around.
		return loadedId; //No swizzle no op.
	}

	uint32_t xSource = swizzle & D3DVS_X_W;
	uint32_t ySource = swizzle & D3DVS_Y_W;
	uint32_t zSource = swizzle & D3DVS_Z_W;
	uint32_t wSource = swizzle & D3DVS_W_W;
	uint32_t outputId = GetNextId();

	//For some reason the mask comes through as WW for text even though it needs to be XY.
	if (lookingFor == GIVE_ME_VECTOR_2 && mMajorVersion == 1)
	{
		xSource = D3DVS_X_X;
		ySource = D3DVS_Y_Y;
	}

	std::string registerName;

	//TypeDescription vectorType;
	//vectorType.PrimaryType = spv::OpTypeVector;
	//vectorType.SecondaryType = spv::OpTypeFloat;
	//vectorType.ComponentCount = 4;
	//vectorType.StorageClass = spv::StorageClassInput;
	//uint32_t vectorTypeId = GetSpirVTypeId(vectorType);

	mIdTypePairs[outputId] = loadedType;

	registerName = mNameIdPairs[loadedId];
	if (registerName.size())
	{
		//registerName += ".r";
		PushName(outputId, registerName);
	}

	uint32_t xIndex = 0;
	uint32_t yIndex = 0;
	uint32_t zIndex = 0;
	uint32_t wIndex = 0;

	switch (xSource)
	{
	case D3DVS_X_X:
		(xIndex = 0); //Component Literal
		break;
	case D3DVS_X_Y:
		(xIndex = 1); //Component Literal
		break;
	case D3DVS_X_Z:
		(xIndex = 2); //Component Literal
		break;
	case D3DVS_X_W:
		(xIndex = 3); //Component Literal
		break;
	}

	switch (ySource)
	{
	case D3DVS_Y_X:
		(yIndex = 0); //Component Literal
		break;
	case D3DVS_Y_Y:
		(yIndex = 1); //Component Literal
		break;
	case D3DVS_Y_Z:
		(yIndex = 2); //Component Literal
		break;
	case D3DVS_Y_W:
		(yIndex = 3); //Component Literal
		break;
	}

	switch (zSource)
	{
	case D3DVS_Z_X:
		(zIndex = 0); //Component Literal
		break;
	case D3DVS_Z_Y:
		(zIndex = 1); //Component Literal
		break;
	case D3DVS_Z_Z:
		(zIndex = 2); //Component Literal
		break;
	case D3DVS_Z_W:
		(zIndex = 3); //Component Literal
		break;
	}

	switch (wSource)
	{
	case D3DVS_W_X:
		(wIndex = 0); //Component Literal
		break;
	case D3DVS_W_Y:
		(wIndex = 1); //Component Literal
		break;
	case D3DVS_W_Z:
		(wIndex = 2); //Component Literal
		break;
	case D3DVS_W_W:
		(wIndex = 3); //Component Literal
		break;
	}

	switch (outputComponentCount)
	{
	case 1:
	{
		if (xIndex == yIndex && xIndex == zIndex && xIndex == wIndex)
		{
			TypeDescription scalarType;
			scalarType.PrimaryType = loadedType.SecondaryType;
			uint32_t scalarTypeId = GetSpirVTypeId(scalarType);

			mIdTypePairs[outputId] = scalarType;

			PushCompositeExtract(scalarTypeId, outputId, loadedId, xIndex);
		}
		else
		{
			Push(spv::OpVectorShuffle, loadedTypeId, outputId, loadedId, loadedId, xIndex, xIndex, xIndex, xIndex);
		}
	}
	break;
	case 2:
		if (xIndex == yIndex && xIndex == zIndex && xIndex == wIndex)
		{
			TypeDescription scalarType;
			scalarType.PrimaryType = loadedType.SecondaryType;
			uint32_t scalarTypeId = GetSpirVTypeId(scalarType);

			mIdTypePairs[outputId] = scalarType;

			PushCompositeExtract(scalarTypeId, outputId, loadedId, xIndex);
		}
		else
		{
			Push(spv::OpVectorShuffle, loadedTypeId, outputId, loadedId, loadedId, xIndex, yIndex, yIndex, yIndex);
		}
		break;
	case 3:
		if (xIndex == yIndex && xIndex == zIndex && xIndex == wIndex)
		{
			TypeDescription scalarType;
			scalarType.PrimaryType = loadedType.SecondaryType;
			uint32_t scalarTypeId = GetSpirVTypeId(scalarType);

			mIdTypePairs[outputId] = scalarType;

			PushCompositeExtract(scalarTypeId, outputId, loadedId, xIndex);
		}
		else
		{
			Push(spv::OpVectorShuffle, loadedTypeId, outputId, loadedId, loadedId, xIndex, yIndex, zIndex, zIndex);
		}
		break;
	case 4:
		if (xIndex == yIndex && xIndex == zIndex && xIndex == wIndex)
		{
			TypeDescription scalarType;
			scalarType.PrimaryType = loadedType.SecondaryType;
			uint32_t scalarTypeId = GetSpirVTypeId(scalarType);

			mIdTypePairs[outputId] = scalarType;

			PushCompositeExtract(scalarTypeId, outputId, loadedId, xIndex);
		}
		else
		{
			Push(spv::OpVectorShuffle, loadedTypeId, outputId, loadedId, loadedId, xIndex, yIndex, zIndex, wIndex);
		}
		break;
	default:
		Log(warning) << "GetSwizzledId - Unsupported component count  " << outputComponentCount << std::endl;
		break;
	}

	return outputId;
}

/*
If we are for example setting xyz on a vec4 we need an access chain to store the correct values in the right spot.
We use the write mask on the result token to figure out out many components we are storing and which ones.
*/
uint32_t ShaderConverter::SwizzlePointer(const Token& token)
{
	uint32_t originalId = GetIdByRegister(token);
	TypeDescription originalType = GetTypeByRegister(token);
	//TypeDescription inputType = mIdTypePairs[originalId];
	TypeDescription outputType;
	uint32_t outputTypeId;
	uint32_t outputId = 0;
	uint32_t outputComponentCount = 0;

	if (token.i & D3DSP_WRITEMASK_0)
	{
		outputComponentCount++;
	}

	if (token.i & D3DSP_WRITEMASK_1)
	{
		outputComponentCount++;
	}

	if (token.i & D3DSP_WRITEMASK_2)
	{
		outputComponentCount++;
	}

	if (token.i & D3DSP_WRITEMASK_3)
	{
		outputComponentCount++;
	}

	if (originalType.SecondaryType != spv::OpTypeVector && outputComponentCount == 1)
	{
		return originalId;
	}

	if (originalType.SecondaryType == spv::OpTypeVector && originalType.ComponentCount == outputComponentCount)
	{
		return originalId;
	}

	if (outputComponentCount > 1)
	{
		return originalId;
	}
	else
	{
		outputType.PrimaryType = spv::OpTypePointer;
		outputType.SecondaryType = originalType.TernaryType;
		outputTypeId = GetSpirVTypeId(outputType);
	}

	if (originalType == outputType)
	{
		return originalId;
	}

	if (token.i & D3DSP_WRITEMASK_0)
	{
		outputId = PushAccessChain(originalId, 0);
	}
	else if (token.i & D3DSP_WRITEMASK_1)
	{
		outputId = PushAccessChain(originalId, 1);
	}
	else if (token.i & D3DSP_WRITEMASK_2)
	{
		outputId = PushAccessChain(originalId, 2);
	}
	else if (token.i & D3DSP_WRITEMASK_3)
	{
		outputId = PushAccessChain(originalId, 3);
	}
	else
	{
		outputId = originalId;
	}

	return outputId;
}


/*
This method takes the result of an operation and does something with it.
If it's just a register it will replace the id in the register dictionary so later calls will get this result instead of an earlier generation.
If it's a pointer though we'll need to do a store to save the result. If it's a color we do some special magic.
*/
uint32_t ShaderConverter::ApplyWriteMask(const Token& token, uint32_t modifiedId, _D3DDECLUSAGE usage)
{
	TypeDescription modifiedType = mIdTypePairs[modifiedId];
	TypeDescription originalType = GetTypeByRegister(token);
	uint32_t originalId = GetIdByRegister(token);
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(token.i);

	uint32_t inputId = 0;
	uint32_t outputId = 0;

	//TypeDescription intType;
	//intType.PrimaryType = spv::OpTypeInt;
	//uint32_t intTypeId = GetSpirVTypeId(intType);

	//TypeDescription intPointerType;
	//intPointerType.PrimaryType = spv::OpTypePointer;
	//intPointerType.SecondaryType = spv::OpTypeInt;
	//uint32_t intPointerTypeId = GetSpirVTypeId(intPointerType);

	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	TypeDescription floatPointerType;
	floatPointerType.PrimaryType = spv::OpTypePointer;
	floatPointerType.SecondaryType = spv::OpTypeFloat;
	uint32_t floatPointerTypeId = GetSpirVTypeId(floatPointerType);

	//If the input is a pointer go ahead and load it. Otherwise we'll just roll with what was passed in.
	if (modifiedType.PrimaryType == spv::OpTypePointer)
	{
		TypeDescription loadedModifiedType;
		loadedModifiedType.PrimaryType = modifiedType.SecondaryType;
		loadedModifiedType.SecondaryType = modifiedType.TernaryType;
		loadedModifiedType.TernaryType = spv::OpTypeVoid;
		loadedModifiedType.ComponentCount = modifiedType.ComponentCount;
		uint32_t loadedModifiedTypeId = GetSpirVTypeId(loadedModifiedType);

		inputId = PushLoad(modifiedId);
	}
	else
	{
		inputId = modifiedId;
	}

	//inputId = SwizzleValue(token, inputId); //This code isn't needed now that we store per channel.

	/*
	If this token has the _sat modifier then do a clamp before we store the result.
	*/
	if (token.i & D3DSPDM_SATURATE)
	{
		uint32_t saturatedInputId = GetNextId();
		TypeDescription saturatedInputType = mIdTypePairs[inputId];
		uint32_t saturatedInputTypeId = GetSpirVTypeId(saturatedInputType);
		mIdTypePairs[saturatedInputId] = saturatedInputType;

		if (saturatedInputType.PrimaryType == spv::OpTypeVector)
		{
			if (saturatedInputType.SecondaryType == spv::OpTypeFloat)
			{
				Push(spv::OpExtInst, saturatedInputTypeId, saturatedInputId, mGlslExtensionId, GLSLstd450::GLSLstd450FClamp, inputId, m0vfId, m1vfId);
			}
			else
			{
				Log(warning) << "ShaderConverter::ApplyWriteMask - unsupported data type for clamp!" << std::endl;
			}
		}
		else if (saturatedInputType.PrimaryType == spv::OpTypeFloat)
		{
			Push(spv::OpExtInst, saturatedInputTypeId, saturatedInputId, mGlslExtensionId, GLSLstd450::GLSLstd450FClamp, inputId, m0fId, m1fId);
		}
		else if (saturatedInputType.PrimaryType == spv::OpTypeInt)
		{
			Push(spv::OpExtInst, saturatedInputTypeId, saturatedInputId, mGlslExtensionId, GLSLstd450::GLSLstd450UClamp, inputId, mConstantIntegerIds[0], mConstantIntegerIds[1]);
		}
		else
		{
			Log(warning) << "ShaderConverter::ApplyWriteMask - unsupported data type for clamp!" << std::endl;
		}

		inputId = saturatedInputId;
	}

	if (token.i & D3DSPDM_PARTIALPRECISION)
	{
		Log(warning) << "ShaderConverter::ApplyWriteMask - D3DSPDM_PARTIALPRECISION is not supported!" << std::endl;
	}

	if (token.i & D3DSPDM_MSAMPCENTROID)
	{
		Log(warning) << "ShaderConverter::ApplyWriteMask - D3DSPDM_MSAMPCENTROID is not supported!" << std::endl;
	}

	/*
	For some reason you can write the result of TEX back to the texture register in DXBC.
	This makes Spir-V angry because input registers are read-only.
	So my work around is to make sure that if the target is an input register to just replace the id with the new one so later calls will pick up the result.
	*/
	if (originalType.StorageClass == spv::StorageClassInput)
	{
		outputId = inputId;
		SetIdByRegister(token, outputId);
	}
	else if (originalType.PrimaryType == spv::OpTypePointer)
	{
		outputId = SwizzlePointer(token);

		if (((token.i & D3DSP_WRITEMASK_ALL) == D3DSP_WRITEMASK_ALL) && (originalType.ComponentCount == modifiedType.ComponentCount))
		{
			TypeDescription inputType = mIdTypePairs[inputId];
			TypeDescription outputType = mIdTypePairs[outputId];
			PushStore(outputId, inputId);
		}
		else
		{
			TypeDescription swizzledType = mIdTypePairs[inputId];
			if (swizzledType.PrimaryType == spv::OpTypeVector)
			{
				/*
				Based on a glsl bug report doing a shuffle and then storing it like I used to do is some kind of race condition.
				To avoid any UB I've switched to the recommended method of storing each compoenent.
				https://github.com/KhronosGroup/glslang/issues/94
				*/
				if (token.i & D3DSP_WRITEMASK_0)
				{
					uint32_t objectId1 = PushCompositeExtract(inputId, 0);
					uint32_t pointerId1 = PushAccessChain(originalId, 0);

					PushStore(pointerId1, objectId1);
				}

				if (token.i & D3DSP_WRITEMASK_1)
				{
					uint32_t objectId2 = PushCompositeExtract(inputId, 1);
					uint32_t pointerId2 = PushAccessChain(originalId, 1);

					PushStore(pointerId2, objectId2);
				}

				if (token.i & D3DSP_WRITEMASK_2)
				{
					uint32_t objectId3 = PushCompositeExtract(inputId, 2);
					uint32_t pointerId3 = PushAccessChain(originalId, 2);

					PushStore(pointerId3, objectId3);
				}

				if (token.i & D3DSP_WRITEMASK_3)
				{
					uint32_t objectId4 = PushCompositeExtract(inputId, 3);
					uint32_t pointerId4 = PushAccessChain(originalId, 3);

					PushStore(pointerId4, objectId4);
				}
			}
			else
			{
				uint32_t objectId1 = inputId;

				if (token.i & D3DSP_WRITEMASK_0)
				{
					uint32_t pointerId1 = PushAccessChain(originalId, 0);

					PushStore(pointerId1, objectId1);
				}

				if (token.i & D3DSP_WRITEMASK_1)
				{
					uint32_t pointerId2 = PushAccessChain(originalId, 1);

					PushStore(pointerId2, objectId1);
				}

				if (token.i & D3DSP_WRITEMASK_2)
				{
					uint32_t pointerId3 = PushAccessChain(originalId, 2);

					PushStore(pointerId3, objectId1);
				}

				if (token.i & D3DSP_WRITEMASK_3)
				{
					uint32_t pointerId4 = PushAccessChain(originalId, 3);

					PushStore(pointerId4, objectId1);
				}
			}
		}
	}
	else
	{
		outputId = originalId;
		SetIdByRegister(token, outputId);
	}

	return outputId;
}

void ShaderConverter::GenerateConstantIndices()
{
	std::string registerName;
	uint32_t stringWordSize = 0;

	//Create float constants.
	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	TypeDescription compositeType;
	compositeType.PrimaryType = spv::OpTypeVector;
	compositeType.SecondaryType = spv::OpTypeFloat;
	compositeType.ComponentCount = 4;
	uint32_t compositeTypeId = GetSpirVTypeId(compositeType);

	m0fId = GetNextId();
	mIdTypePairs[m0fId] = floatType;
	mTypeInstructions.push_back(Pack(3 + 1, spv::OpConstant)); //size,Type
	mTypeInstructions.push_back(floatTypeId); //Result Type (Id)
	mTypeInstructions.push_back(m0fId); //Result (Id)
	mTypeInstructions.push_back(0); //Literal Value
	registerName = "float_0";
	PushName(m0fId, registerName);

	m0vfId = GetNextId();
	mIdTypePairs[m0vfId] = compositeType;
	mTypeInstructions.push_back(Pack(3 + 4, spv::OpConstantComposite)); //size,Type
	mTypeInstructions.push_back(compositeTypeId); //Result Type (Id)
	mTypeInstructions.push_back(m0vfId); //Result (Id)
	mTypeInstructions.push_back(m0fId); // Value (Id)
	mTypeInstructions.push_back(m0fId); // Value (Id)
	mTypeInstructions.push_back(m0fId); // Value (Id)
	mTypeInstructions.push_back(m0fId); // Value (Id)
	registerName = "vec4_0";
	PushName(m0vfId, registerName);

	m1fId = GetNextId();
	mIdTypePairs[m1fId] = floatType;
	mTypeInstructions.push_back(Pack(3 + 1, spv::OpConstant)); //size,Type
	mTypeInstructions.push_back(floatTypeId); //Result Type (Id)
	mTypeInstructions.push_back(m1fId); //Result (Id)
	mTypeInstructions.push_back(bit_cast(1.0f)); //Literal Value
	registerName = "float_1";
	PushName(m1fId, registerName);

	mn1fId = GetNextId();
	mIdTypePairs[mn1fId] = floatType;
	mTypeInstructions.push_back(Pack(3 + 1, spv::OpConstant)); //size,Type
	mTypeInstructions.push_back(floatTypeId); //Result Type (Id)
	mTypeInstructions.push_back(mn1fId); //Result (Id)
	mTypeInstructions.push_back(bit_cast(-1.0f)); //Literal Value
	registerName = "float_-1";
	PushName(mn1fId, registerName);

	m1vfId = GetNextId();
	mIdTypePairs[m1vfId] = compositeType;
	mTypeInstructions.push_back(Pack(3 + 4, spv::OpConstantComposite)); //size,Type
	mTypeInstructions.push_back(compositeTypeId); //Result Type (Id)
	mTypeInstructions.push_back(m1vfId); //Result (Id)
	mTypeInstructions.push_back(m1fId); // Value (Id)
	mTypeInstructions.push_back(m1fId); // Value (Id)
	mTypeInstructions.push_back(m1fId); // Value (Id)
	mTypeInstructions.push_back(m1fId); // Value (Id)
	registerName = "vec4_1";
	PushName(m1vfId, registerName);

	//Create int constants.
	TypeDescription intType;
	intType.PrimaryType = spv::OpTypeInt;
	uint32_t intTypeId = GetSpirVTypeId(intType);

	for (int32_t i = 0; i < 17; i++)
	{
		mConstantIntegerIds[i] = GetNextId();
		mIdTypePairs[mConstantIntegerIds[i]] = intType;
		mTypeInstructions.push_back(Pack(3 + 1, spv::OpConstant)); //size,Type
		mTypeInstructions.push_back(intTypeId); //Result Type (Id)
		mTypeInstructions.push_back(mConstantIntegerIds[i]); //Result (Id)
		mTypeInstructions.push_back(i); //Literal Value
		registerName = "int_" + i;
		PushName(mConstantIntegerIds[i], registerName);
	}
}

void ShaderConverter::GenerateStore(const Token& token, uint32_t inputId)
{
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(token.i);
	uint32_t resultId = GetNextVersionId(token);
	uint32_t argumentId1 = inputId;

	switch (resultRegisterType)
	{
	case D3DSPR_RASTOUT:
	case D3DSPR_ATTROUT:
	case D3DSPR_COLOROUT:
	case D3DSPR_DEPTHOUT:
	case D3DSPR_OUTPUT:
		PushStore(resultId, argumentId1);
		break;
	default:
		break;
	}
}

void ShaderConverter::GenerateDecoration(uint32_t registerNumber, uint32_t inputId, _D3DDECLUSAGE usage, uint32_t usageIndex, bool isInput)
{
	std::string registerName;
	uint32_t location;
	//TypeDescription inputType = mIdTypePairs[inputId];

	if (this->mIsVertexShader)
	{
		if (isInput)
		{
			location = UsageOffsets[usage] + usageIndex;

			mDecorateInstructions.push_back(Pack(3 + 1, spv::OpDecorate)); //size,Type
			mDecorateInstructions.push_back(inputId); //target (Id)
			mDecorateInstructions.push_back(spv::DecorationLocation); //Decoration Type (Id)
			mDecorateInstructions.push_back(location); //Location offset

			registerName = "v" + std::to_string(registerNumber);
		}
		else
		{
			switch (mMajorVersion)
			{
			case 1:
				location = UsageOffsets[usage] + usageIndex;
				break;
			case 2:
				location = UsageOffsets[usage] + usageIndex;
				break;
			case 3:
				location = registerNumber;
				break;
			default:
				location = UsageOffsets[usage] + usageIndex;
				break;
			}

			if (usage == D3DDECLUSAGE_POSITION)
			{
				//generate builtin for oPos
				mDecorateInstructions.push_back(Pack(3 + 1, spv::OpDecorate)); //size,Type
				mDecorateInstructions.push_back(inputId); //target (Id)
				mDecorateInstructions.push_back(spv::DecorationBuiltIn); //Decoration Type (Id)
				mDecorateInstructions.push_back(spv::BuiltInPosition); //Location offset

				mPositionYId = PushAccessChain(inputId, 1);
			}
			else
			{
				mDecorateInstructions.push_back(Pack(3 + 1, spv::OpDecorate)); //size,Type
				mDecorateInstructions.push_back(inputId); //target (Id)
				mDecorateInstructions.push_back(spv::DecorationLocation); //Decoration Type (Id)
				mDecorateInstructions.push_back(location); //Location offset
			}

			switch (usage)
			{
			case D3DDECLUSAGE_POSITION:
				registerName = "oPos";
				break;
			case D3DDECLUSAGE_FOG:
				registerName = "oFog";
				break;
			case D3DDECLUSAGE_PSIZE:
				registerName = "oPts";
				break;
			case D3DDECLUSAGE_COLOR:
				registerName = "oD" + std::to_string(registerNumber);
				break;
			case D3DDECLUSAGE_TEXCOORD:
				registerName = "oT" + std::to_string(registerNumber);
				break;
			default:
				registerName = "o" + std::to_string(registerNumber);
				break;
			}
		}
	}
	else //Pixel Shader
	{
		if (isInput)
		{
			switch (mMajorVersion)
			{
			case 1:
				location = UsageOffsets[usage] + usageIndex;
				break;
			case 2:
				location = UsageOffsets[usage] + usageIndex;
				break;
			case 3:
				location = registerNumber;
				break;
			default:
				location = UsageOffsets[usage] + usageIndex;
				break;
			}

			mDecorateInstructions.push_back(Pack(3 + 1, spv::OpDecorate)); //size,Type
			mDecorateInstructions.push_back(inputId); //target (Id)
			mDecorateInstructions.push_back(spv::DecorationLocation); //Decoration Type (Id)
			mDecorateInstructions.push_back(location); //Location offset

			switch (usage)
			{
			case D3DDECLUSAGE_TEXCOORD:
				registerName = "t" + std::to_string(registerNumber);
				break;
			case D3DDECLUSAGE_COLOR:
				registerName = "v" + std::to_string(registerNumber);
				break;
			default:
				registerName = "v" + std::to_string(registerNumber);
				break;
			}
		}
		else
		{
			if (usage == D3DDECLUSAGE_COLOR)
			{
				mDecorateInstructions.push_back(Pack(3 + 1, spv::OpDecorate)); //size,Type
				mDecorateInstructions.push_back(inputId); //target (Id)
				mDecorateInstructions.push_back(spv::DecorationLocation); //Decoration Type (Id)
				mDecorateInstructions.push_back(usageIndex); //Location offset
			}
			else
			{
				mDecorateInstructions.push_back(Pack(3 + 1, spv::OpDecorate)); //size,Type
				mDecorateInstructions.push_back(inputId); //target (Id)
				mDecorateInstructions.push_back(spv::DecorationLocation); //Decoration Type (Id)
				mDecorateInstructions.push_back(2 + usageIndex); //Location offset
			}

			switch (usage)
			{

			case D3DDECLUSAGE_COLOR:
				registerName = "oC" + std::to_string(registerNumber);
				break;
			case D3DDECLUSAGE_DEPTH:
				registerName = "oDepth";
				break;
			default:
				registerName = "o" + std::to_string(registerNumber);
				break;
			}
		}
	}
	PushName(inputId, registerName);

	Log(info) << "ShaderConverter::GenerateDecoration created " << registerName << " for " << usage << " @ " << location << std::endl;
}

void ShaderConverter::Generate255Constants()
{
	uint32_t id = GetNextId();
	TypeDescription type;
	type.PrimaryType = spv::OpTypeFloat;
	uint32_t typeId = GetSpirVTypeId(type);

	mIdTypePairs[id] = type;

	mTypeInstructions.push_back(Pack(3 + 1, spv::OpConstant)); //size,Type
	mTypeInstructions.push_back(typeId); //Result Type (Id)
	mTypeInstructions.push_back(id); //Result (Id)
	mTypeInstructions.push_back(bit_cast(255.0f)); //Literal Value

	uint32_t compositeId = GetNextId();
	TypeDescription compositeType;
	compositeType.PrimaryType = spv::OpTypeVector;
	compositeType.SecondaryType = spv::OpTypeFloat;
	compositeType.ComponentCount = 4;
	uint32_t compositeTypeId = GetSpirVTypeId(compositeType);

	mIdTypePairs[compositeId] = compositeType;

	mTypeInstructions.push_back(Pack(3 + 4, spv::OpConstantComposite)); //size,Type
	mTypeInstructions.push_back(compositeTypeId); //Result Type (Id)
	mTypeInstructions.push_back(compositeId); //Result (Id)
	mTypeInstructions.push_back(id); // Value (Id)
	mTypeInstructions.push_back(id); // Value (Id)
	mTypeInstructions.push_back(id); // Value (Id)
	mTypeInstructions.push_back(id); // Value (Id)

	m255FloatId = id;
	m255VectorId = compositeId;
}

void ShaderConverter::GenerateYFlipConstants()
{
	uint32_t compositeId = GetNextId();
	TypeDescription compositeType;
	compositeType.PrimaryType = spv::OpTypeVector;
	compositeType.SecondaryType = spv::OpTypeFloat;
	compositeType.ComponentCount = 4;
	uint32_t compositeTypeId = GetSpirVTypeId(compositeType);

	mIdTypePairs[compositeId] = compositeType;

	mTypeInstructions.push_back(Pack(3 + 4, spv::OpConstantComposite)); //size,Type
	mTypeInstructions.push_back(compositeTypeId); //Result Type (Id)
	mTypeInstructions.push_back(compositeId); //Result (Id)
	mTypeInstructions.push_back(m1fId); // Value (Id)
	mTypeInstructions.push_back(mn1fId); // Value (Id)
	mTypeInstructions.push_back(m1fId); // Value (Id)
	mTypeInstructions.push_back(m1fId); // Value (Id)

	mYFlipVectorId = compositeId;
}

void ShaderConverter::GenerateConstantBlock()
{
	//Fetch Id values for types we'll need later.
	TypeDescription integerType;
	integerType.PrimaryType = spv::OpTypeInt;
	uint32_t integerTypeId = GetSpirVTypeId(integerType);

	TypeDescription integerVectorType;
	integerVectorType.PrimaryType = spv::OpTypeVector;
	integerVectorType.SecondaryType = spv::OpTypeInt;
	integerVectorType.ComponentCount = 4;
	uint32_t integerVectorTypeId = GetSpirVTypeId(integerVectorType);

	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	TypeDescription floatVectorType;
	floatVectorType.PrimaryType = spv::OpTypeVector;
	floatVectorType.SecondaryType = spv::OpTypeFloat;
	floatVectorType.ComponentCount = 4;
	uint32_t floatVectorTypeId = GetSpirVTypeId(floatVectorType);

	//Generate Ids up front.
	uint32_t uboStructureTypeId = GetNextId();
	uint32_t uboStructurePointerTypeId = GetNextId();
	mUboPointerId = GetNextId();

	//Create structure layout
	mDecorateInstructions.push_back(Pack(3, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationBlock); //Decoration Type (Id)

	mDecorateInstructions.push_back(Pack(4, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationDescriptorSet); //Decoration Type (Id)
	mDecorateInstructions.push_back(0); //descriptor set index

	mDecorateInstructions.push_back(Pack(4, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationBinding); //Decoration Type (Id)
	if (mIsVertexShader)
	{
		mDecorateInstructions.push_back(5); //binding index.
	}
	else
	{
		mDecorateInstructions.push_back(6); //binding index.
	}

	mTypeInstructions.push_back(Pack(2 + (mNumberOfIntegerVectorsInUbo + mNumberOfBooleansInUbo + mNumberOfFloatVectorsInUbo), spv::OpTypeStruct)); //size,Type
	mTypeInstructions.push_back(uboStructureTypeId); //Result (Id)

	uint32_t memberIndex = 0;
	uint32_t memberOffset = 0;
	for (int32_t i = 0; i < (int32_t)mNumberOfIntegerVectorsInUbo; i++)
	{
		mTypeInstructions.push_back(integerVectorTypeId);

		mDecorateInstructions.push_back(Pack(4 + 1, spv::OpMemberDecorate)); //size,Type
		mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
		mDecorateInstructions.push_back(memberIndex); //Member (Literal)
		mDecorateInstructions.push_back(spv::DecorationOffset); //Decoration Type (Id)
		mDecorateInstructions.push_back(memberOffset);

		memberIndex += 1;
		memberOffset += (sizeof(int) * 4);
	}

	for (int32_t i = 0; i < (int32_t)mNumberOfBooleansInUbo; i++)
	{
		mTypeInstructions.push_back(integerTypeId);

		mDecorateInstructions.push_back(Pack(4 + 1, spv::OpMemberDecorate)); //size,Type
		mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
		mDecorateInstructions.push_back(memberIndex); //Member (Literal)
		mDecorateInstructions.push_back(spv::DecorationOffset); //Decoration Type (Id)
		mDecorateInstructions.push_back(memberOffset);

		memberIndex += 1;
		memberOffset += sizeof(int); //using int to avoid extr alignment troubles.
	}

	for (int32_t i = 0; i < (int32_t)mNumberOfFloatVectorsInUbo; i++)
	{
		mTypeInstructions.push_back(floatVectorTypeId);

		mDecorateInstructions.push_back(Pack(4 + 1, spv::OpMemberDecorate)); //size,Type
		mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
		mDecorateInstructions.push_back(memberIndex); //Member (Literal)
		mDecorateInstructions.push_back(spv::DecorationOffset); //Decoration Type (Id)
		mDecorateInstructions.push_back(memberOffset);

		memberIndex += 1;
		memberOffset += (sizeof(float) * 4);
	}

	//Create pointer type with layout.
	mTypeInstructions.push_back(Pack(4, spv::OpTypePointer)); //size,Type
	mTypeInstructions.push_back(uboStructurePointerTypeId); //Result (Id)
	mTypeInstructions.push_back(spv::StorageClassUniform); //Storage Class
	mTypeInstructions.push_back(uboStructureTypeId); //type (Id)

	//Create variable with pointer type.
	mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
	mTypeInstructions.push_back(uboStructurePointerTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
	mTypeInstructions.push_back(mUboPointerId); //Result (Id)
	mTypeInstructions.push_back(spv::StorageClassUniform); //Storage Class

	//Add variable Name
	std::string variableName = "shaderConstants";
	PushName(mUboPointerId, variableName);
}

void ShaderConverter::GenerateRenderStateBlock()
{
	//Fetch Id values for types we'll need later.
	TypeDescription integerType;
	integerType.PrimaryType = spv::OpTypeInt;
	uint32_t integerTypeId = GetSpirVTypeId(integerType);

	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	//Generate Ids up front.
	uint32_t uboStructureTypeId = GetNextId();
	uint32_t uboStructurePointerTypeId = GetNextId();
	mRenderStatePointerId = GetNextId();

	//Create structure layout
	mDecorateInstructions.push_back(Pack(3, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationBlock); //Decoration Type (Id)

	mDecorateInstructions.push_back(Pack(4, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationDescriptorSet); //Decoration Type (Id)
	mDecorateInstructions.push_back(0); //descriptor set index

	mDecorateInstructions.push_back(Pack(4, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationBinding); //Decoration Type (Id)
	mDecorateInstructions.push_back(0); //binding index.

	constexpr uint32_t numberofMembers = (D3DRS_BLENDOPALPHA + 1);

	mTypeInstructions.push_back(Pack(2 + numberofMembers, spv::OpTypeStruct)); //size,Type
	mTypeInstructions.push_back(uboStructureTypeId); //Result (Id)

	uint32_t memberIndex = 0;
	uint32_t memberOffset = 0;
	for (int32_t i = 0; i < numberofMembers; i++)
	{
		//if (i < 18)
		//{
		//	mTypeInstructions.push_back(floatTypeId);
		//}
		//else
		//{
		//	mTypeInstructions.push_back(integerTypeId);
		//}
		mTypeInstructions.push_back(integerTypeId);

		mDecorateInstructions.push_back(Pack(4 + 1, spv::OpMemberDecorate)); //size,Type
		mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
		mDecorateInstructions.push_back(memberIndex); //Member (Literal)
		mDecorateInstructions.push_back(spv::DecorationOffset); //Decoration Type (Id)
		mDecorateInstructions.push_back(memberOffset);

		memberIndex += 1;
		memberOffset += sizeof(uint32_t);
	}

	//Create pointer type with layout.
	mTypeInstructions.push_back(Pack(4, spv::OpTypePointer)); //size,Type
	mTypeInstructions.push_back(uboStructurePointerTypeId); //Result (Id)
	mTypeInstructions.push_back(spv::StorageClassUniform); //Storage Class
	mTypeInstructions.push_back(uboStructureTypeId); //type (Id)

	//Create variable with pointer type.
	mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
	mTypeInstructions.push_back(uboStructurePointerTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
	mTypeInstructions.push_back(mRenderStatePointerId); //Result (Id)
	mTypeInstructions.push_back(spv::StorageClassUniform); //Storage Class

	//Add variable Name
	std::string variableName = "renderState";
	PushName(mRenderStatePointerId, variableName);
}

void ShaderConverter::GenerateSamplerBlock()
{
	//Generate Ids up front.
	uint32_t uboStructurePointerTypeId = GetNextId();
	mTexturesId = GetNextId();

	//Make Types
	TypeDescription imageType;
	imageType.PrimaryType = spv::OpTypePointer;
	imageType.SecondaryType = spv::OpTypeImage;
	imageType.StorageClass = spv::StorageClassInput;
	uint32_t imageTypeId = GetSpirVTypeId(imageType);

	TypeDescription imageArrayType;
	imageArrayType.PrimaryType = spv::OpTypeArray;
	imageArrayType.SecondaryType = spv::OpTypeImage;
	imageArrayType.StorageClass = spv::StorageClassInput;
	imageArrayType.ComponentCount = 16;
	uint32_t imageArrayTypeId = GetSpirVTypeId(imageArrayType);

	//Create structure layout
	mDecorateInstructions.push_back(Pack(4, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(imageArrayTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationDescriptorSet); //Decoration Type (Id)
	mDecorateInstructions.push_back(0); //descriptor set index

	mDecorateInstructions.push_back(Pack(4, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(imageArrayTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationBinding); //Decoration Type (Id)
	mDecorateInstructions.push_back(7); //binding index.

	//Create pointer type with layout.
	mTypeInstructions.push_back(Pack(4, spv::OpTypePointer)); //size,Type
	mTypeInstructions.push_back(uboStructurePointerTypeId); //Result (Id)
	mTypeInstructions.push_back(spv::StorageClassUniformConstant); //Storage Class
	mTypeInstructions.push_back(imageArrayTypeId); //type (Id)

	//Create variable with pointer type.
	mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
	mTypeInstructions.push_back(uboStructurePointerTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
	mTypeInstructions.push_back(mTexturesId); //Result (Id)
	mTypeInstructions.push_back(spv::StorageClassUniformConstant); //Storage Class

	//Add variable Name
	std::string variableName = "textures";
	PushName(mTexturesId, variableName);
}

void ShaderConverter::GenerateTextureStageBlock()
{
	//Fetch Id values for types we'll need later.
	TypeDescription integerType;
	integerType.PrimaryType = spv::OpTypeInt;
	uint32_t integerTypeId = GetSpirVTypeId(integerType);

	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	TypeDescription matrixType;
	matrixType.PrimaryType = spv::OpTypeMatrix;
	matrixType.SecondaryType = spv::OpTypeVector;
	matrixType.ComponentCount = 4;
	uint32_t matrixTypeId = GetSpirVTypeId(matrixType);

	//Generate Ids up front.
	uint32_t uboStructureTypeId = GetNextId();
	uint32_t uboStructurePointerTypeId = GetNextId();
	uint32_t uboStructureArrayTypeId = GetNextId();
	mTextureStagesId = GetNextId();

	mTextureStageTypeId = uboStructureTypeId;

	//Create structure layout
	mDecorateInstructions.push_back(Pack(3, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationBlock); //Decoration Type (Id)

	mDecorateInstructions.push_back(Pack(4, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationDescriptorSet); //Decoration Type (Id)
	mDecorateInstructions.push_back(0); //descriptor set index

	mDecorateInstructions.push_back(Pack(4, spv::OpDecorate)); //size,Type
	mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
	mDecorateInstructions.push_back(spv::DecorationBinding); //Decoration Type (Id)
	mDecorateInstructions.push_back(1); //binding index.

	constexpr uint32_t numberofMembers = (D3DTSS_CONSTANT + 1);

	mTypeInstructions.push_back(Pack(2 + numberofMembers, spv::OpTypeStruct)); //size,Type
	mTypeInstructions.push_back(uboStructureTypeId); //Result (Id)

	uint32_t memberIndex = 0;
	uint32_t memberOffset = 0;
	for (int32_t i = 0; i < numberofMembers; i++)
	{
		int32_t offset = 0;

		//if (i < 1)
		//{
		//	mTypeInstructions.push_back(matrixTypeId);
		//	offset = sizeof(D3DMATRIX);
		//}
		//else if (i < 13)
		//{
		//	mTypeInstructions.push_back(integerTypeId);
		//	offset = sizeof(uint32_t);
		//}
		//else
		//{
		//	mTypeInstructions.push_back(floatTypeId);
		//	offset = sizeof(float);
		//}

		mTypeInstructions.push_back(integerTypeId);
		offset = sizeof(uint32_t);

		mDecorateInstructions.push_back(Pack(4 + 1, spv::OpMemberDecorate)); //size,Type
		mDecorateInstructions.push_back(uboStructureTypeId); //target (Id)
		mDecorateInstructions.push_back(memberIndex); //Member (Literal)
		mDecorateInstructions.push_back(spv::DecorationOffset); //Decoration Type (Id)
		mDecorateInstructions.push_back(memberOffset);

		memberIndex += 1;
		memberOffset += offset;
	}

	//Create array type.
	mTypeInstructions.push_back(Pack(4, spv::OpTypeArray)); //size,Type
	mTypeInstructions.push_back(uboStructureArrayTypeId); //Id
	mTypeInstructions.push_back(uboStructureTypeId); // Type
	mTypeInstructions.push_back(mConstantIntegerIds[16]); // Length

	//Create pointer type with layout.
	mTypeInstructions.push_back(Pack(4, spv::OpTypePointer)); //size,Type
	mTypeInstructions.push_back(uboStructurePointerTypeId); //Result (Id)
	mTypeInstructions.push_back(spv::StorageClassUniform); //Storage Class
	mTypeInstructions.push_back(uboStructureArrayTypeId); //type (Id)

	//Create variable with pointer type.
	mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
	mTypeInstructions.push_back(uboStructurePointerTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
	mTypeInstructions.push_back(mTextureStagesId); //Result (Id)
	mTypeInstructions.push_back(spv::StorageClassUniform); //Storage Class

	//Add variable Name
	std::string variableName = "textureStages";
	PushName(mTextureStagesId, variableName);
}

void ShaderConverter::GenerateYFlip()
{
	if (!mPositionYId)
	{
		return;
	}

	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	uint32_t positionYId = PushLoad(mPositionYId);

	uint32_t resultId = GetNextId();
	mIdTypePairs[resultId] = floatType;
	Push(spv::OpFMul, floatTypeId, resultId, positionYId, mn1fId);
	PushStore(mPositionYId, resultId);
}

void ShaderConverter::CombineSpirVOpCodes()
{
	mInstructions.insert(std::end(mInstructions), std::begin(mCapabilityInstructions), std::end(mCapabilityInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mExtensionInstructions), std::end(mExtensionInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mImportExtendedInstructions), std::end(mImportExtendedInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mMemoryModelInstructions), std::end(mMemoryModelInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mEntryPointInstructions), std::end(mEntryPointInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mExecutionModeInstructions), std::end(mExecutionModeInstructions));

	mInstructions.insert(std::end(mInstructions), std::begin(mStringInstructions), std::end(mStringInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mSourceExtensionInstructions), std::end(mSourceExtensionInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mSourceInstructions), std::end(mSourceInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mSourceContinuedInstructions), std::end(mSourceContinuedInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mNameInstructions), std::end(mNameInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mMemberNameInstructions), std::end(mMemberNameInstructions));

	mInstructions.insert(std::end(mInstructions), std::begin(mDecorateInstructions), std::end(mDecorateInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mMemberDecorateInstructions), std::end(mMemberDecorateInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mGroupDecorateInstructions), std::end(mGroupDecorateInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mGroupMemberDecorateInstructions), std::end(mGroupMemberDecorateInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mDecorationGroupInstructions), std::end(mDecorationGroupInstructions));

	mInstructions.insert(std::end(mInstructions), std::begin(mTypeInstructions), std::end(mTypeInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mFunctionDeclarationInstructions), std::end(mFunctionDeclarationInstructions));
	mInstructions.insert(std::end(mInstructions), std::begin(mFunctionDefinitionInstructions), std::end(mFunctionDefinitionInstructions));

	mCapabilityInstructions.clear();
	mExtensionInstructions.clear();
	mImportExtendedInstructions.clear();
	mMemoryModelInstructions.clear();
	mEntryPointInstructions.clear();
	mExecutionModeInstructions.clear();

	mStringInstructions.clear();
	mSourceExtensionInstructions.clear();
	mSourceInstructions.clear();
	mSourceContinuedInstructions.clear();
	mNameInstructions.clear();
	mMemberNameInstructions.clear();

	mDecorateInstructions.clear();
	mMemberDecorateInstructions.clear();
	mGroupDecorateInstructions.clear();
	mGroupMemberDecorateInstructions.clear();
	mDecorationGroupInstructions.clear();

	mTypeInstructions.clear();
	mFunctionDeclarationInstructions.clear();
	mFunctionDefinitionInstructions.clear();
}

vk::UniqueShaderModule ShaderConverter::CreateSpirVModule()
{
	vk::ShaderModuleCreateInfo moduleCreateInfo;
	moduleCreateInfo.codeSize = mInstructions.size() * sizeof(uint32_t);
	moduleCreateInfo.pCode = mInstructions.data(); //Why is this uint32_t* if the size is in bytes?

#ifdef _EXTRA_SHADER_DEBUG_INFO
	std::stringstream stream;
	if (!mIsVertexShader)
	{
		stream << "fragment_" << std::hex << this << ".spv";
	}
	else
	{
		stream << "vertex_" << std::hex << this << ".spv";
	}

	std::ofstream outFile(stream.str(), std::ios::out | std::ios::binary);
	outFile.write((char*)mInstructions.data(), mInstructions.size() * sizeof(uint32_t));
#endif

	return mDevice.createShaderModuleUnique(moduleCreateInfo);
}

uint32_t ShaderConverter::FixColor(uint32_t id)
{
	TypeDescription originalType = mIdTypePairs[id];
	uint32_t originalTypeId = GetSpirVTypeId(originalType);

	TypeDescription loadedType;
	uint32_t loadedTypeId;
	uint32_t loadedId;
	if (originalType.PrimaryType == spv::OpTypePointer)
	{
		loadedType = GetValueType(originalType);
		loadedTypeId = GetSpirVTypeId(loadedType);
		loadedId = PushLoad(id);
	}
	else
	{
		loadedType = originalType;
		loadedTypeId = originalTypeId;
		loadedId = id;
	}

	TypeDescription newType = loadedType;
	newType.SecondaryType = spv::OpTypeFloat;
	uint32_t newTypeId = GetSpirVTypeId(newType);

	uint32_t convertedId = GetNextId();
	mIdTypePairs[convertedId] = newType;
	Push(spv::OpConvertUToF, newTypeId, convertedId, loadedId);

	uint32_t newId = GetNextId();
	mIdTypePairs[newId] = newType;
	Push(spv::OpFDiv, newTypeId, newId, convertedId, m255VectorId);

	return newId;
}

uint32_t ShaderConverter::FixPosition(uint32_t id)
{
	TypeDescription originalType = mIdTypePairs[id];
	uint32_t originalTypeId = GetSpirVTypeId(originalType);

	TypeDescription loadedType;
	uint32_t loadedTypeId;
	uint32_t loadedId;
	if (originalType.PrimaryType == spv::OpTypePointer)
	{
		loadedType = GetValueType(originalType);
		loadedTypeId = GetSpirVTypeId(loadedType);
		loadedId = PushLoad(id);
	}
	else
	{
		loadedType = originalType;
		loadedTypeId = originalTypeId;
		loadedId = id;
	}

	TypeDescription newType = loadedType;
	newType.SecondaryType = spv::OpTypeFloat;
	uint32_t newTypeId = GetSpirVTypeId(newType);

	//Fix w
	uint32_t convertedId = GetNextId();
	mIdTypePairs[convertedId] = newType;
	Push(spv::OpCompositeInsert, newTypeId, convertedId, m1fId, loadedId, 3);

	//I need to flip on the output so matrix multiplication works.
	//uint32_t newId = GetNextId();
	//mIdTypePairs[newId] = newType;
	//Push(spv::OpFMul, newTypeId, newId, convertedId, mYFlipVectorId);

	return convertedId;
}

/*
To match GLSL behavior we will extract the components and construct a new composite from the pieces.
*/

uint32_t ShaderConverter::ConvertVec4ToVec3(uint32_t id)
{
	//Check to see if the input is a pointer and if so load it first.
	TypeDescription originalType = mIdTypePairs[id];
	uint32_t originalTypeId = GetSpirVTypeId(originalType);

	uint32_t loadedId = 0;
	TypeDescription loadedType;
	uint32_t loadedTypeId = 0;

	if (originalType.PrimaryType == spv::OpTypePointer)
	{
		loadedType.PrimaryType = originalType.SecondaryType;
		loadedType.SecondaryType = originalType.TernaryType;
		loadedType.TernaryType = spv::OpTypeVoid;
		loadedType.ComponentCount = originalType.ComponentCount;
		uint32_t loadedTypeId = GetSpirVTypeId(loadedType);

		loadedId = PushLoad(id);
	}
	else
	{
		loadedType = originalType;
		loadedId = id;
		loadedTypeId = originalTypeId;
	}

	//Finally build the new vector.
	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	TypeDescription vectorType;
	vectorType.PrimaryType = spv::OpTypeVector;
	vectorType.SecondaryType = spv::OpTypeFloat;
	vectorType.ComponentCount = 3;
	uint32_t vectorTypeId = GetSpirVTypeId(vectorType);

	//
	uint32_t xId = PushCompositeExtract(loadedId, 0);
	uint32_t yId = PushCompositeExtract(loadedId, 1);
	uint32_t zId = PushCompositeExtract(loadedId, 1);

	//
	uint32_t resultId = GetNextId();
	mIdTypePairs[resultId] = vectorType;
	Push(spv::OpCompositeConstruct, vectorTypeId, resultId, xId, yId, zId);

	return resultId;
}

uint32_t ShaderConverter::ConvertMat4ToMat3(uint32_t id)
{
	//Check to see if the input is a pointer and if so load it first.
	TypeDescription originalType = mIdTypePairs[id];
	uint32_t originalTypeId = GetSpirVTypeId(originalType);

	uint32_t loadedId = 0;
	TypeDescription loadedType;
	uint32_t loadedTypeId = 0;

	if (originalType.PrimaryType == spv::OpTypePointer)
	{
		loadedType.PrimaryType = originalType.SecondaryType;
		loadedType.SecondaryType = originalType.TernaryType;
		loadedType.TernaryType = spv::OpTypeVoid;
		loadedType.ComponentCount = originalType.ComponentCount;
		loadedTypeId = GetSpirVTypeId(loadedType);

		loadedId = PushLoad(id);
	}
	else
	{
		loadedType = originalType;
		loadedId = id;
		loadedTypeId = originalTypeId;
	}

	//Finally build the new matrix.
	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	TypeDescription vectorType;
	vectorType.PrimaryType = spv::OpTypeVector;
	vectorType.SecondaryType = spv::OpTypeFloat;
	vectorType.ComponentCount = 3;
	uint32_t vectorTypeId = GetSpirVTypeId(vectorType);

	TypeDescription matrixType;
	matrixType.PrimaryType = spv::OpTypeMatrix;
	matrixType.SecondaryType = spv::OpTypeVector;
	matrixType.ComponentCount = 3;
	uint32_t matrixTypeId = GetSpirVTypeId(matrixType);

	//
	uint32_t x1Id = PushCompositeExtract2(loadedId, 0, 0);
	uint32_t y1Id = PushCompositeExtract2(loadedId, 0, 1);
	uint32_t z1Id = PushCompositeExtract2(loadedId, 0, 2);

	uint32_t v1Id = GetNextId();
	mIdTypePairs[v1Id] = vectorType;
	Push(spv::OpCompositeConstruct, vectorTypeId, v1Id, x1Id, y1Id, z1Id);

	//
	uint32_t x2Id = PushCompositeExtract2(loadedId, 1, 0);
	uint32_t y2Id = PushCompositeExtract2(loadedId, 1, 1);
	uint32_t z2Id = PushCompositeExtract2(loadedId, 1, 2);

	uint32_t v2Id = GetNextId();
	mIdTypePairs[v2Id] = vectorType;
	Push(spv::OpCompositeConstruct, vectorTypeId, v2Id, x2Id, y2Id, z2Id);

	//
	uint32_t x3Id = PushCompositeExtract2(loadedId, 2, 0);
	uint32_t y3Id = PushCompositeExtract2(loadedId, 2, 1);
	uint32_t z3Id = PushCompositeExtract2(loadedId, 2, 2);

	uint32_t v3Id = GetNextId();
	mIdTypePairs[v3Id] = vectorType;
	Push(spv::OpCompositeConstruct, vectorTypeId, v3Id, x3Id, y3Id, z3Id);

	//
	uint32_t resultId = GetNextId();
	mIdTypePairs[resultId] = matrixType;
	Push(spv::OpCompositeConstruct, matrixTypeId, resultId, v1Id, v2Id, v3Id);

	return resultId;
}

void ShaderConverter::PushMemberName(uint32_t id, std::string& registerName, uint32_t index)
{
	std::vector<uint32_t> nameInstructions;
	PutStringInVector(registerName, nameInstructions); //Literal

	mNameInstructions.push_back(Pack(nameInstructions.size() + 3, spv::OpMemberName));
	mNameInstructions.push_back(id); //target (Id)
	mNameInstructions.push_back(index);
	mNameInstructions.insert(mNameInstructions.end(), nameInstructions.begin(), nameInstructions.end());

	mNameIdPairs[id] = registerName;
}

void ShaderConverter::PushName(uint32_t id, std::string& registerName)
{
	std::vector<uint32_t> nameInstructions;
	PutStringInVector(registerName, nameInstructions); //Literal

	mNameInstructions.push_back(Pack(nameInstructions.size() + 2, spv::OpName));
	mNameInstructions.push_back(id); //target (Id)
	mNameInstructions.insert(mNameInstructions.end(), nameInstructions.begin(), nameInstructions.end());

	mNameIdPairs[id] = registerName;
}

uint32_t ShaderConverter::PushCompositeExtract(uint32_t baseId, uint32_t index)
{
	uint32_t resultId = GetNextId();

	PushCompositeExtract(resultId, baseId, index);

	return resultId;
}

uint32_t ShaderConverter::PushCompositeExtract(uint32_t resultId, uint32_t baseId, uint32_t index)
{
	TypeDescription baseType = mIdTypePairs[baseId];
	uint32_t baseTypeId = GetSpirVTypeId(baseType);

	TypeDescription resultType = GetComponentType(baseType);
	uint32_t resultTypeId = GetSpirVTypeId(resultType);

	mIdTypePairs[resultId] = resultType;

	PushCompositeExtract(resultTypeId, resultId, baseId, index);

	return resultId;
}

void ShaderConverter::PushCompositeExtract(uint32_t resultTypeId, uint32_t resultId, uint32_t baseId, uint32_t index)
{
#ifdef _EXTRA_SHADER_DEBUG_INFO
	if (resultTypeId == 0)
	{
		throw "";
	}
#endif

	std::string registerName = mNameIdPairs[baseId];
	if (registerName.size())
	{
		if (index == 0)
		{
			registerName += ".x";
		}
		else if (index == 1)
		{
			registerName += ".y";
		}
		else if (index == 2)
		{
			registerName += ".z";
		}
		else if (index == 3)
		{
			registerName += ".w";
		}

		PushName(resultId, registerName);
	}

	Push(spv::OpCompositeExtract, resultTypeId, resultId, baseId, index);
}

uint32_t ShaderConverter::PushCompositeExtract2(uint32_t baseId, uint32_t index1, uint32_t index2)
{
	uint32_t resultId = GetNextId();

	PushCompositeExtract2(resultId, baseId, index1, index2);

	return resultId;
}

uint32_t ShaderConverter::PushCompositeExtract2(uint32_t resultId, uint32_t baseId, uint32_t index1, uint32_t index2)
{
	TypeDescription baseType = mIdTypePairs[baseId];
	uint32_t baseTypeId = GetSpirVTypeId(baseType);

	TypeDescription resultType = GetComponentType(baseType);
	uint32_t resultTypeId = GetSpirVTypeId(resultType);

	mIdTypePairs[resultId] = resultType;

	PushCompositeExtract2(resultTypeId, resultId, baseId, index1, index2);

	return resultId;
}

void ShaderConverter::PushCompositeExtract2(uint32_t resultTypeId, uint32_t resultId, uint32_t baseId, uint32_t index1, uint32_t index2)
{
#ifdef _EXTRA_SHADER_DEBUG_INFO
	if (resultTypeId == 0)
	{
		throw "";
	}
#endif

	std::string registerName = mNameIdPairs[baseId];
	if (registerName.size())
	{
		registerName += "[" + std::to_string(index1) + "][" + std::to_string(index2) + "]";

		PushName(resultId, registerName);
	}

	Push(spv::OpCompositeExtract, resultTypeId, resultId, baseId, index1, index2);
}

uint32_t ShaderConverter::PushAccessChain(uint32_t baseId, uint32_t index)
{
	uint32_t resultId = GetNextId();

	PushAccessChain(resultId, baseId, index);

	return resultId;
}

uint32_t ShaderConverter::PushAccessChain(uint32_t resultId, uint32_t baseId, uint32_t index)
{
	TypeDescription baseType = mIdTypePairs[baseId];
	uint32_t baseTypeId = GetSpirVTypeId(baseType);

	TypeDescription resultType = GetPointerComponentType(baseType);
	uint32_t resultTypeId = GetSpirVTypeId(resultType);

	mIdTypePairs[resultId] = resultType;

	PushAccessChain(resultTypeId, resultId, baseId, mConstantIntegerIds[index]);

	return resultId;
}

void ShaderConverter::PushAccessChain(uint32_t resultTypeId, uint32_t resultId, uint32_t baseId, uint32_t indexId)
{
#ifdef _EXTRA_SHADER_DEBUG_INFO
	if (resultTypeId == 0)
	{
		throw "";
	}
#endif

	std::string registerName = mNameIdPairs[baseId];
	if (registerName.size())
	{
		if (indexId == mConstantIntegerIds[0])
		{
			registerName += "[0]";
		}
		else if (indexId == mConstantIntegerIds[1])
		{
			registerName += "[1]";
		}
		else if (indexId == mConstantIntegerIds[2])
		{
			registerName += "[2]";
		}
		else if (indexId == mConstantIntegerIds[3])
		{
			registerName += "[3]";
		}

		PushName(resultId, registerName);
	}

	Push(spv::OpAccessChain, resultTypeId, resultId, baseId, indexId);
}

void ShaderConverter::PushInverseSqrt(uint32_t resultTypeId, uint32_t resultId, uint32_t argumentId)
{
#ifdef _EXTRA_SHADER_DEBUG_INFO
	if (resultTypeId == 0)
	{
		throw "";
	}

	auto& argumentType = mIdTypePairs[argumentId];
	auto& resultType = mIdTypePairs[resultTypeId];

	if (resultType.PrimaryType != argumentType.PrimaryType)
	{
		Log(fatal) << "ShaderConverter::PushInverseSqrt PrimaryType mismatch " << resultType.PrimaryType << " != " << argumentType.PrimaryType << std::endl;
	}

	if (resultType.SecondaryType != argumentType.SecondaryType)
	{
		Log(fatal) << "ShaderConverter::PushInverseSqrt SecondaryType mismatch " << resultType.SecondaryType << " != " << argumentType.SecondaryType << std::endl;
	}

	if (resultType.TernaryType != argumentType.TernaryType)
	{
		Log(fatal) << "ShaderConverter::PushInverseSqrt TernaryType mismatch " << resultType.TernaryType << " != " << argumentType.TernaryType << std::endl;
	}

	if (resultType.ComponentCount != argumentType.ComponentCount)
	{
		Log(fatal) << "ShaderConverter::PushInverseSqrt ComponentCount mismatch " << resultType.ComponentCount << " != " << argumentType.ComponentCount << std::endl;
	}
#endif

	Push(spv::OpExtInst, resultTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450InverseSqrt, argumentId);
}

void ShaderConverter::PushCos(uint32_t resultTypeId, uint32_t resultId, uint32_t argumentId)
{
#ifdef _EXTRA_SHADER_DEBUG_INFO
	if (resultTypeId == 0)
	{
		throw "";
	}

	auto& argumentType = mIdTypePairs[argumentId];
	auto& resultType = mIdTypePairs[resultTypeId];

	if (resultType.PrimaryType != argumentType.PrimaryType)
	{
		Log(fatal) << "ShaderConverter::PushCos PrimaryType mismatch " << resultType.PrimaryType << " != " << argumentType.PrimaryType << std::endl;
	}

	if (resultType.SecondaryType != argumentType.SecondaryType)
	{
		Log(fatal) << "ShaderConverter::PushCos SecondaryType mismatch " << resultType.SecondaryType << " != " << argumentType.SecondaryType << std::endl;
	}

	if (resultType.TernaryType != argumentType.TernaryType)
	{
		Log(fatal) << "ShaderConverter::PushCos TernaryType mismatch " << resultType.TernaryType << " != " << argumentType.TernaryType << std::endl;
	}

	if (resultType.ComponentCount != argumentType.ComponentCount)
	{
		Log(fatal) << "ShaderConverter::PushCos ComponentCount mismatch " << resultType.ComponentCount << " != " << argumentType.ComponentCount << std::endl;
	}
#endif

	Push(spv::OpExtInst, resultTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Cos, argumentId);
}

void ShaderConverter::PushSin(uint32_t resultTypeId, uint32_t resultId, uint32_t argumentId)
{
#ifdef _EXTRA_SHADER_DEBUG_INFO
	if (resultTypeId == 0)
	{
		throw "";
	}

	auto& argumentType = mIdTypePairs[argumentId];
	auto& resultType = mIdTypePairs[resultTypeId];

	if (resultType.PrimaryType != argumentType.PrimaryType)
	{
		Log(fatal) << "ShaderConverter::PushSin PrimaryType mismatch " << resultType.PrimaryType << " != " << argumentType.PrimaryType << std::endl;
	}

	if (resultType.SecondaryType != argumentType.SecondaryType)
	{
		Log(fatal) << "ShaderConverter::PushSin SecondaryType mismatch " << resultType.SecondaryType << " != " << argumentType.SecondaryType << std::endl;
	}

	if (resultType.TernaryType != argumentType.TernaryType)
	{
		Log(fatal) << "ShaderConverter::PushSin TernaryType mismatch " << resultType.TernaryType << " != " << argumentType.TernaryType << std::endl;
	}

	if (resultType.ComponentCount != argumentType.ComponentCount)
	{
		Log(fatal) << "ShaderConverter::PushSin ComponentCount mismatch " << resultType.ComponentCount << " != " << argumentType.ComponentCount << std::endl;
	}
#endif

	Push(spv::OpExtInst, resultTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Sin, argumentId);
}

uint32_t ShaderConverter::PushLoad(uint32_t pointerId)
{
	uint32_t resultId = GetNextId();

	PushLoad(resultId, pointerId);

	return resultId;
}

uint32_t ShaderConverter::PushLoad(uint32_t resultId, uint32_t pointerId)
{
	TypeDescription pointerType = mIdTypePairs[pointerId];
	uint32_t pointerTypeId = GetSpirVTypeId(pointerType);

	TypeDescription resultType = GetValueType(pointerType);
	uint32_t resultTypeId = GetSpirVTypeId(resultType);

	mIdTypePairs[resultId] = resultType;

	PushLoad(resultTypeId, resultId, pointerId);

	return resultId;
}

void ShaderConverter::PushLoad(uint32_t resultTypeId, uint32_t resultId, uint32_t pointerId)
{
#ifdef _EXTRA_SHADER_DEBUG_INFO
	if (resultTypeId == 0)
	{
		throw "";
	}

	auto& resultType = mIdTypePairs[resultTypeId];
	auto& pointerType = mIdTypePairs[pointerId];

	if (pointerType.PrimaryType != spv::OpTypePointer)
	{
		Log(fatal) << "ShaderConverter::PushLoad PrimaryType is not OpPointer " << pointerType.PrimaryType << std::endl;
	}

	if (pointerType.SecondaryType != resultType.PrimaryType)
	{
		Log(fatal) << "ShaderConverter::PushLoad PrimaryType mismatch " << pointerType.SecondaryType << " != " << resultType.PrimaryType << std::endl;
	}

	if (pointerType.TernaryType != resultType.SecondaryType)
	{
		Log(fatal) << "ShaderConverter::PushLoad SecondaryType mismatch " << pointerType.TernaryType << " != " << resultType.SecondaryType << std::endl;
	}

	if (pointerType.ComponentCount != resultType.ComponentCount)
	{
		Log(fatal) << "ShaderConverter::PushLoad ComponentCount mismatch " << pointerType.ComponentCount << " != " << resultType.ComponentCount << std::endl;
	}
#endif

	std::string registerName = mNameIdPairs[pointerId];
	if (registerName.size())
	{
		registerName += "_loaded";
		PushName(resultId, registerName);
	}

	Push(spv::OpLoad, resultTypeId, resultId, pointerId);
}

void ShaderConverter::PushStore(uint32_t pointerId, uint32_t objectId)
{
#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& objectType = mIdTypePairs[objectId];
	auto& pointerType = mIdTypePairs[pointerId];

	if (pointerType.PrimaryType != spv::OpTypePointer)
	{
		Log(fatal) << "ShaderConverter::PushStore PrimaryType is not OpPointer " << pointerType.PrimaryType << std::endl;
	}

	if (pointerType.SecondaryType != objectType.PrimaryType)
	{
		Log(fatal) << "ShaderConverter::PushStore PrimaryType mismatch " << pointerType.SecondaryType << " != " << objectType.PrimaryType << std::endl;
	}

	if (pointerType.TernaryType != objectType.SecondaryType)
	{
		Log(fatal) << "ShaderConverter::PushStore SecondaryType mismatch " << pointerType.TernaryType << " != " << objectType.SecondaryType << std::endl;
	}

	if (pointerType.ComponentCount != objectType.ComponentCount)
	{
		Log(fatal) << "ShaderConverter::PushStore ComponentCount mismatch " << pointerType.ComponentCount << " != " << objectType.ComponentCount << std::endl;
	}
#endif

	Push(spv::OpStore, pointerId, objectId);
}

void ShaderConverter::PushVariable(uint32_t resultTypeId, uint32_t resultId, spv::StorageClass storageClass)
{
	mTypeInstructions.push_back(Pack(4, spv::OpVariable));
	mTypeInstructions.push_back(resultTypeId);
	mTypeInstructions.push_back(resultId);
	mTypeInstructions.push_back(storageClass);
}

void ShaderConverter::Push(spv::Op code)
{
	mFunctionDefinitionInstructions.push_back(Pack(1, code)); //size,Type

	Log(info) << "ShaderConverter::Push " << code << std::endl;
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1)
{
	mFunctionDefinitionInstructions.push_back(Pack(2, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2)
{
	mFunctionDefinitionInstructions.push_back(Pack(3, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << std::endl;
#endif	
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3)
{
	mFunctionDefinitionInstructions.push_back(Pack(4, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << std::endl;
#endif	
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, uint32_t argument4)
{
	mFunctionDefinitionInstructions.push_back(Pack(5, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, uint32_t argument4, uint32_t argument5)
{
	mFunctionDefinitionInstructions.push_back(Pack(6, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);
	mFunctionDefinitionInstructions.push_back(argument5);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];
	auto& argumentType5 = mIdTypePairs[argument5];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << ", " << argument5 << "(" << argumentType5 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, GLSLstd450 argument4, uint32_t argument5)
{
	mFunctionDefinitionInstructions.push_back(Pack(6, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);
	mFunctionDefinitionInstructions.push_back(argument5);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];
	auto& argumentType5 = mIdTypePairs[argument5];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << ", " << argument5 << "(" << argumentType5 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, uint32_t argument4, uint32_t argument5, uint32_t argument6)
{
	mFunctionDefinitionInstructions.push_back(Pack(7, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);
	mFunctionDefinitionInstructions.push_back(argument5);
	mFunctionDefinitionInstructions.push_back(argument6);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];
	auto& argumentType5 = mIdTypePairs[argument5];
	auto& argumentType6 = mIdTypePairs[argument6];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << ", " << argument5 << "(" << argumentType5 << ")" << ", " << argument6 << "(" << argumentType6 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, GLSLstd450 argument4, uint32_t argument5, uint32_t argument6)
{
	mFunctionDefinitionInstructions.push_back(Pack(7, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);
	mFunctionDefinitionInstructions.push_back(argument5);
	mFunctionDefinitionInstructions.push_back(argument6);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];
	auto& argumentType5 = mIdTypePairs[argument5];
	auto& argumentType6 = mIdTypePairs[argument6];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << ", " << argument5 << "(" << argumentType5 << ")" << ", " << argument6 << "(" << argumentType6 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, uint32_t argument4, uint32_t argument5, uint32_t argument6, uint32_t argument7)
{
	mFunctionDefinitionInstructions.push_back(Pack(8, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);
	mFunctionDefinitionInstructions.push_back(argument5);
	mFunctionDefinitionInstructions.push_back(argument6);
	mFunctionDefinitionInstructions.push_back(argument7);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];
	auto& argumentType5 = mIdTypePairs[argument5];
	auto& argumentType6 = mIdTypePairs[argument6];
	auto& argumentType7 = mIdTypePairs[argument7];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << ", " << argument5 << "(" << argumentType5 << ")" << ", " << argument6 << "(" << argumentType6 << ")" << ", " << argument7 << "(" << argumentType7 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, uint32_t argument4, uint32_t argument5, uint32_t argument6, uint32_t argument7, uint32_t argument8)
{
	mFunctionDefinitionInstructions.push_back(Pack(9, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);
	mFunctionDefinitionInstructions.push_back(argument5);
	mFunctionDefinitionInstructions.push_back(argument6);
	mFunctionDefinitionInstructions.push_back(argument7);
	mFunctionDefinitionInstructions.push_back(argument8);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];
	auto& argumentType5 = mIdTypePairs[argument5];
	auto& argumentType6 = mIdTypePairs[argument6];
	auto& argumentType7 = mIdTypePairs[argument7];
	auto& argumentType8 = mIdTypePairs[argument8];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << ", " << argument5 << "(" << argumentType5 << ")" << ", " << argument6 << "(" << argumentType6 << ")" << ", " << argument7 << "(" << argumentType7 << ")" << ", " << argument8 << "(" << argumentType8 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, uint32_t argument4, uint32_t argument5, uint32_t argument6, uint32_t argument7, uint32_t argument8, uint32_t argument9)
{
	mFunctionDefinitionInstructions.push_back(Pack(10, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);
	mFunctionDefinitionInstructions.push_back(argument5);
	mFunctionDefinitionInstructions.push_back(argument6);
	mFunctionDefinitionInstructions.push_back(argument7);
	mFunctionDefinitionInstructions.push_back(argument8);
	mFunctionDefinitionInstructions.push_back(argument9);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];
	auto& argumentType5 = mIdTypePairs[argument5];
	auto& argumentType6 = mIdTypePairs[argument6];
	auto& argumentType7 = mIdTypePairs[argument7];
	auto& argumentType8 = mIdTypePairs[argument8];
	auto& argumentType9 = mIdTypePairs[argument9];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << ", " << argument5 << "(" << argumentType5 << ")" << ", " << argument6 << "(" << argumentType6 << ")" << ", " << argument7 << "(" << argumentType7 << ")" << ", " << argument8 << "(" << argumentType8 << ")" << ", " << argument9 << "(" << argumentType9 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, uint32_t argument4, uint32_t argument5, uint32_t argument6, uint32_t argument7, uint32_t argument8, uint32_t argument9, uint32_t argument10)
{
	mFunctionDefinitionInstructions.push_back(Pack(11, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);
	mFunctionDefinitionInstructions.push_back(argument5);
	mFunctionDefinitionInstructions.push_back(argument6);
	mFunctionDefinitionInstructions.push_back(argument7);
	mFunctionDefinitionInstructions.push_back(argument8);
	mFunctionDefinitionInstructions.push_back(argument9);
	mFunctionDefinitionInstructions.push_back(argument10);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];
	auto& argumentType5 = mIdTypePairs[argument5];
	auto& argumentType6 = mIdTypePairs[argument6];
	auto& argumentType7 = mIdTypePairs[argument7];
	auto& argumentType8 = mIdTypePairs[argument8];
	auto& argumentType9 = mIdTypePairs[argument9];
	auto& argumentType10 = mIdTypePairs[argument10];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << ", " << argument5 << "(" << argumentType5 << ")" << ", " << argument6 << "(" << argumentType6 << ")" << ", " << argument7 << "(" << argumentType7 << ")" << ", " << argument8 << "(" << argumentType8 << ")" << ", " << argument9 << "(" << argumentType9 << ")" << ", " << argument10 << "(" << argumentType10 << ")" << std::endl;
#endif
}

void ShaderConverter::Push(spv::Op code, uint32_t argument1, uint32_t argument2, uint32_t argument3, uint32_t argument4, uint32_t argument5, uint32_t argument6, uint32_t argument7, uint32_t argument8, uint32_t argument9, uint32_t argument10, uint32_t argument11)
{
	mFunctionDefinitionInstructions.push_back(Pack(12, code)); //size,Type
	mFunctionDefinitionInstructions.push_back(argument1);
	mFunctionDefinitionInstructions.push_back(argument2);
	mFunctionDefinitionInstructions.push_back(argument3);
	mFunctionDefinitionInstructions.push_back(argument4);
	mFunctionDefinitionInstructions.push_back(argument5);
	mFunctionDefinitionInstructions.push_back(argument6);
	mFunctionDefinitionInstructions.push_back(argument7);
	mFunctionDefinitionInstructions.push_back(argument8);
	mFunctionDefinitionInstructions.push_back(argument9);
	mFunctionDefinitionInstructions.push_back(argument10);
	mFunctionDefinitionInstructions.push_back(argument11);

#ifdef _EXTRA_SHADER_DEBUG_INFO
	auto& argumentType1 = mIdTypePairs[argument1];
	auto& argumentType2 = mIdTypePairs[argument2];
	auto& argumentType3 = mIdTypePairs[argument3];
	auto& argumentType4 = mIdTypePairs[argument4];
	auto& argumentType5 = mIdTypePairs[argument5];
	auto& argumentType6 = mIdTypePairs[argument6];
	auto& argumentType7 = mIdTypePairs[argument7];
	auto& argumentType8 = mIdTypePairs[argument8];
	auto& argumentType9 = mIdTypePairs[argument9];
	auto& argumentType10 = mIdTypePairs[argument10];
	auto& argumentType11 = mIdTypePairs[argument11];

	Log(info) << "ShaderConverter::Push " << code << " " << argument1 << "(" << argumentType1 << ")" << ", " << argument2 << "(" << argumentType2 << ")" << ", " << argument3 << "(" << argumentType3 << ")" << ", " << argument4 << "(" << argumentType4 << ")" << ", " << argument5 << "(" << argumentType5 << ")" << ", " << argument6 << "(" << argumentType6 << ")" << ", " << argument7 << "(" << argumentType7 << ")" << ", " << argument8 << "(" << argumentType8 << ")" << ", " << argument9 << "(" << argumentType9 << ")" << ", " << argument10 << "(" << argumentType10 << ")" << ", " << argument11 << "(" << argumentType11 << ")" << std::endl;
#endif
}

void ShaderConverter::Process_DCL_Pixel()
{
	Token token = GetNextToken();
	Token registerToken = GetNextToken();
	uint32_t usage = GetUsage(token.i);
	uint32_t usageIndex = GetUsageIndex(token.i);
	uint32_t registerNumber = GetRegisterNumber(registerToken);
	_D3DSHADER_PARAM_REGISTER_TYPE registerType = GetRegisterType(registerToken.i);
	uint32_t tokenId = GetNextVersionId(registerToken);
	TypeDescription typeDescription;
	uint32_t registerComponents = (registerToken.i & D3DSP_WRITEMASK_ALL) >> 16;
	uint32_t resultTypeId;
	//uint32_t textureType;
	//std::string registerName;

	if (registerType == D3DSPR_SAMPLER)
	{
		typeDescription.PrimaryType = spv::OpTypePointer;
		typeDescription.SecondaryType = spv::OpTypeImage;
		typeDescription.StorageClass = spv::StorageClassUniformConstant;
	}
	else
	{
		typeDescription.PrimaryType = spv::OpTypePointer;
		typeDescription.StorageClass = spv::StorageClassInput;

		//Magic numbers from ToGL (no whole numbers?)
		switch (registerComponents)
		{
		case 1: //float
			typeDescription.SecondaryType = spv::OpTypeFloat;
			typeDescription.TernaryType = spv::OpTypeVoid;
			typeDescription.ComponentCount = 1;
			break;
		case 3: //vec2
			//This is really a vec2 but I'm going to declare it has a vec4 to make later code easier.	
			typeDescription.SecondaryType = spv::OpTypeVector;
			typeDescription.TernaryType = spv::OpTypeFloat;
			//typeDescription.ComponentCount = 2;
			typeDescription.ComponentCount = 4;
			break;
		case 7: //vec3
			//This is really a vec3 but I'm going to declare it has a vec4 to make later code easier.		
			typeDescription.SecondaryType = spv::OpTypeVector;
			typeDescription.TernaryType = spv::OpTypeFloat;
			//typeDescription.ComponentCount = 3;
			typeDescription.ComponentCount = 4;
			break;
		case 0xF: //vec4
			typeDescription.SecondaryType = spv::OpTypeVector;
			typeDescription.TernaryType = spv::OpTypeFloat;
			typeDescription.ComponentCount = 4;
			break;
		default:
			Log(warning) << "Process_DCL - Unsupported component type " << registerComponents << std::endl;
			break;
		}
	}

	mIdTypePairs[tokenId] = typeDescription;

	switch (registerType)
	{
	case D3DSPR_TEXTURE:
	case D3DSPR_INPUT:
		typeDescription.StorageClass = spv::StorageClassInput;
		resultTypeId = GetSpirVTypeId(typeDescription);

		mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
		mTypeInstructions.push_back(resultTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
		mTypeInstructions.push_back(tokenId); //Result (Id)
		mTypeInstructions.push_back(spv::StorageClassInput); //Storage Class
		//Optional initializer

		mInputRegisters.push_back(tokenId);

		GenerateDecoration(registerNumber, tokenId, (_D3DDECLUSAGE)usage, usageIndex, true);
		break;
	case D3DSPR_SAMPLER:
		typeDescription.PrimaryType = spv::OpTypePointer;
		typeDescription.SecondaryType = spv::OpTypeImage;
		typeDescription.StorageClass = spv::StorageClassUniformConstant;
		resultTypeId = GetSpirVTypeId(typeDescription);

		PushAccessChain(tokenId, mTexturesId, registerNumber);
		break;
	case D3DSPR_TEMP:
		if (registerNumber != 0) //r0 is used for pixel shader color output because reasons.
		{
			typeDescription.StorageClass = spv::StorageClassPrivate; //Storage Class
		}
		else
		{
			GenerateDecoration(registerNumber, tokenId, (_D3DDECLUSAGE)usage, usageIndex, true);
			typeDescription.StorageClass = spv::StorageClassOutput; //Storage Class
		}

		resultTypeId = GetSpirVTypeId(typeDescription);

		mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
		mTypeInstructions.push_back(resultTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
		mTypeInstructions.push_back(tokenId); //Result (Id)

		break;
	case D3DSPR_MISCTYPE:
		Log(fatal) << "ShaderConverter::Process_DCL_Pixel unsupported register type D3DSPR_MISCTYPE" << std::endl;
	default:
		Log(fatal) << "ShaderConverter::Process_DCL_Pixel unsupported register type " << registerType << std::endl;
		break;
	}

	mIdsByRegister[registerType][registerNumber] = tokenId;
	mRegistersById[registerType][tokenId] = registerNumber;
	mIdTypePairs[tokenId] = typeDescription;

	mUsages[tokenId] = (_D3DDECLUSAGE)usage;

	Log(info) << "DCL " << GetUsageName((_D3DDECLUSAGE)usage) << "(" << usageIndex << ") - " << GetRegisterTypeName(registerType) << "(" << registerNumber << ") = " << typeDescription << std::endl;
}

void ShaderConverter::Process_DCL_Vertex()
{
	Token token = GetNextToken();
	Token registerToken = GetNextToken();
	uint32_t usage = GetUsage(token.i);
	uint32_t usageIndex = GetUsageIndex(token.i);
	uint32_t registerNumber = GetRegisterNumber(registerToken);
	_D3DSHADER_PARAM_REGISTER_TYPE registerType = GetRegisterType(registerToken.i);
	uint32_t tokenId = GetNextVersionId(registerToken);
	TypeDescription typeDescription;
	uint32_t registerComponents = (registerToken.i & D3DSP_WRITEMASK_ALL) >> 16;
	uint32_t resultTypeId;
	//uint32_t stringWordSize;

	typeDescription.PrimaryType = spv::OpTypePointer;
	typeDescription.SecondaryType = spv::OpTypeVector;

	//Magic numbers from ToGL
	switch (registerComponents)
	{
	case 1: //float
		if (usage == D3DDECLUSAGE_COLOR && registerType == D3DSPR_INPUT)
		{
			typeDescription.SecondaryType = spv::OpTypeInt;
		}
		else
		{
			typeDescription.SecondaryType = spv::OpTypeFloat;
		}

		typeDescription.TernaryType = spv::OpTypeVoid;
		typeDescription.ComponentCount = 0;
		break;
	case 3: //vec2
		if (usage == D3DDECLUSAGE_COLOR && registerType == D3DSPR_INPUT)
		{
			typeDescription.TernaryType = spv::OpTypeInt;
		}
		else
		{
			typeDescription.TernaryType = spv::OpTypeFloat;
		}

		//This is really a vec2 but I'm going to declare it has a vec4 to make later code easier.
		//typeDescription.ComponentCount = 2;
		typeDescription.ComponentCount = 4;
		break;
	case 7: //vec3
		if (usage == D3DDECLUSAGE_COLOR && registerType == D3DSPR_INPUT)
		{
			typeDescription.TernaryType = spv::OpTypeInt;
		}
		else
		{
			typeDescription.TernaryType = spv::OpTypeFloat;
		}

		//This is really a vec3 but I'm going to declare it has a vec4 to make later code easier.
		//typeDescription.ComponentCount = 3;
		typeDescription.ComponentCount = 4;
		break;
	case 0xF: //vec4
		if (usage == D3DDECLUSAGE_COLOR && registerType == D3DSPR_INPUT)
		{
			typeDescription.TernaryType = spv::OpTypeInt;
		}
		else
		{
			typeDescription.TernaryType = spv::OpTypeFloat;
		}

		typeDescription.ComponentCount = 4;
		break;
	default:
		Log(warning) << "Process_DCL - Unsupported component type " << registerComponents << std::endl;
		break;
	}

	switch (registerType)
	{
	case D3DSPR_TEXTURE:
	case D3DSPR_INPUT:
		resultTypeId = GetSpirVTypeId(typeDescription);

		mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
		mTypeInstructions.push_back(resultTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
		mTypeInstructions.push_back(tokenId); //Result (Id)
		mTypeInstructions.push_back(spv::StorageClassInput); //Storage Class

		mInputRegisters.push_back(tokenId);

		GenerateDecoration(registerNumber, tokenId, (_D3DDECLUSAGE)usage, usageIndex, true);

		break;
	case D3DSPR_RASTOUT:
	case D3DSPR_ATTROUT:
	case D3DSPR_COLOROUT:
	case D3DSPR_DEPTHOUT:
	case D3DSPR_OUTPUT:
		resultTypeId = GetSpirVTypeId(typeDescription);

		mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
		mTypeInstructions.push_back(resultTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
		mTypeInstructions.push_back(tokenId); //Result (Id)
		mTypeInstructions.push_back(spv::StorageClassOutput); //Storage Class
		//Optional initializer

		switch (usage)
		{
		case D3DDECLUSAGE_POSITION:
			mPositionRegister = usageIndex; //might need this later.
			break;
		case D3DDECLUSAGE_COLOR:
			break;
		default:
			break;
		}

		mOutputRegisters.push_back(tokenId);
		mOutputRegisterUsages[(_D3DDECLUSAGE)usage] = tokenId;
		GenerateDecoration(registerNumber, tokenId, (_D3DDECLUSAGE)usage, usageIndex, false);
		break;
	case D3DSPR_TEMP:
		typeDescription.StorageClass = spv::StorageClassPrivate;
		resultTypeId = GetSpirVTypeId(typeDescription);

		mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
		mTypeInstructions.push_back(resultTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
		mTypeInstructions.push_back(tokenId); //Result (Id)
		mTypeInstructions.push_back(spv::StorageClassPrivate); //Storage Class

		break;
	default:
		Log(fatal) << "ShaderConverter::Process_DCL_Vertex unsupported register type " << registerType << std::endl;
		break;
	}

	mIdTypePairs[tokenId] = typeDescription;

	if (registerType == D3DSPR_INPUT)
	{
		switch (usage)
		{
		case D3DDECLUSAGE_COLOR:
			tokenId = FixColor(tokenId);
			break;
		case D3DDECLUSAGE_POSITION:
			tokenId = FixPosition(tokenId);
			break;
		}
	}

	mIdsByRegister[registerType][registerNumber] = tokenId;
	mRegistersById[registerType][tokenId] = registerNumber;

	mUsages[tokenId] = (_D3DDECLUSAGE)usage;

	Log(info) << "DCL " << GetUsageName((_D3DDECLUSAGE)usage) << "(" << usageIndex << ") - " << GetRegisterTypeName(registerType) << "(" << registerNumber << ") = " << typeDescription << std::endl;
}

void ShaderConverter::Process_DCL()
{
	if (mIsVertexShader)
	{
		if (mMajorVersion >= 3)
		{
			Process_DCL_Vertex();
		}
		else if (mMajorVersion >= 2)
		{
			Process_DCL_Vertex();
		}
		else
		{
			Process_DCL_Vertex();
			//Log(warning) << "ShaderConverter::Process_DCL unsupported shader version " << mMajorVersion;
		}
	}
	else
	{
		if (mMajorVersion >= 3)
		{
			Process_DCL_Pixel();
		}
		else if (mMajorVersion >= 2)
		{
			Process_DCL_Pixel();
		}
		else
		{
			Process_DCL_Pixel();
			//Log(warning) << "ShaderConverter::Process_DCL unsupported shader version " << mMajorVersion;
		}
	}
}

void ShaderConverter::Process_DEF()
{
	DWORD literalValue;
	Token token = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE registerType = GetRegisterType(token.i);
	DestinationParameterToken  destinationParameterToken = token.DestinationParameterToken;

	for (int32_t i = 0; i < 4; i++)
	{
		literalValue = GetNextToken().i;
		//mShaderConstantSlots.FloatConstants[token.DestinationParameterToken.RegisterNumber * 4 + i] = bit_cast(literalValue);
	}

	PrintTokenInformation("DEF", token, token, token);
}

void ShaderConverter::Process_DEFI()
{
	DWORD literalValue;
	Token token = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE registerType = GetRegisterType(token.i);
	DestinationParameterToken  destinationParameterToken = token.DestinationParameterToken;

	for (int32_t i = 0; i < 4; i++)
	{
		literalValue = GetNextToken().i;
		//mShaderConstantSlots.IntegerConstants[token.DestinationParameterToken.RegisterNumber * 4 + i] = literalValue;
	}

	PrintTokenInformation("DEFI", token, token, token);
}

void ShaderConverter::Process_DEFB()
{
	DWORD literalValue;
	Token token = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE registerType = GetRegisterType(token.i);
	DestinationParameterToken  destinationParameterToken = token.DestinationParameterToken;

	literalValue = GetNextToken().i;
	//mShaderConstantSlots.BooleanConstants[token.DestinationParameterToken.RegisterNumber * 4] = literalValue;

	PrintTokenInformation("DEFB", token, token, token);
}

void ShaderConverter::Process_IFC(uint32_t extraInfo)
{
	//Token resultToken = GetNextToken();
	//_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	//Grab type info if there is any.
	TypeDescription argumentType1 = mIdTypePairs[argumentId1];
	TypeDescription argumentType2 = mIdTypePairs[argumentId2];
	TypeDescription typeDescription = argumentType2;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	TypeDescription labelType;
	labelType.PrimaryType = spv::OpLabel;
	//uint32_t labelTypeId = GetSpirVTypeId(labelType);

	TypeDescription boolType;
	boolType.PrimaryType = spv::OpTypeBool;
	uint32_t boolTypeId = GetSpirVTypeId(boolType);

	TypeDescription boolVectorType;
	boolVectorType.PrimaryType = spv::OpTypeVector;
	boolVectorType.SecondaryType = spv::OpTypeBool;
	boolVectorType.ComponentCount = 4;
	uint32_t boolVectorTypeId = GetSpirVTypeId(boolVectorType);

	uint32_t resultTypeId;
	spv::Op dataType;
	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
		resultTypeId = boolVectorTypeId;

		mIdTypePairs[resultId] = boolVectorType;
	}
	else
	{
		dataType = typeDescription.PrimaryType;
		resultTypeId = boolTypeId;

		mIdTypePairs[resultId] = boolType;
	}

	switch (extraInfo)
	{
	case 1:
		switch (dataType)
		{
		case spv::OpTypeBool:
			Push(spv::OpUGreaterThan, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeInt:
			Push(spv::OpUGreaterThan, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeFloat:
			Push(spv::OpFOrdGreaterThan, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		default:
			Log(warning) << "Process_IFC - Unsupported data type " << dataType << std::endl;
			break;
		}
		break;
	case 2:
		switch (dataType)
		{
		case spv::OpTypeBool:
			Push(spv::OpIEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeInt:
			Push(spv::OpIEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeFloat:
			Push(spv::OpFOrdEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		default:
			Log(warning) << "Process_IFC - Unsupported data type " << dataType << std::endl;
			break;
		}
		break;
	case 3:
		switch (dataType)
		{
		case spv::OpTypeBool:
			Push(spv::OpUGreaterThanEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeInt:
			Push(spv::OpUGreaterThanEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeFloat:
			Push(spv::OpFOrdGreaterThanEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		default:
			Log(warning) << "Process_IFC - Unsupported data type " << dataType << std::endl;
			break;
		}
		break;
	case 4:
		switch (dataType)
		{
		case spv::OpTypeBool:
			Push(spv::OpULessThan, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeInt:
			Push(spv::OpULessThan, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeFloat:
			Push(spv::OpFOrdLessThan, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		default:
			Log(warning) << "Process_IFC - Unsupported data type " << dataType << std::endl;
			break;
		}
		break;
	case 5:
		switch (dataType)
		{
		case spv::OpTypeBool:
			Push(spv::OpINotEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeInt:
			Push(spv::OpINotEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeFloat:
			Push(spv::OpFOrdNotEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		default:
			Log(warning) << "Process_IFC - Unsupported data type " << dataType << std::endl;
			break;
		}
		break;
	case 6:
		switch (dataType)
		{
		case spv::OpTypeBool:
			Push(spv::OpULessThanEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeInt:
			Push(spv::OpULessThanEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		case spv::OpTypeFloat:
			Push(spv::OpFOrdLessThanEqual, resultTypeId, resultId, argumentId1, argumentId2);
			break;
		default:
			Log(warning) << "Process_IFC - Unsupported data type " << dataType << std::endl;
			break;
		}
		break;
	default:
		Log(warning) << "Process_IFC - Unsupported compare type " << extraInfo << std::endl;
		break;
	}

	uint32_t trueLabelId = GetNextId();
	uint32_t falseLabelId = GetNextId();
	uint32_t endIfLabelId = GetNextId();

	mIdTypePairs[trueLabelId] = labelType;
	mIdTypePairs[falseLabelId] = labelType;
	mIdTypePairs[endIfLabelId] = labelType;

	mFalseLabels.push(falseLabelId);
	mFalseLabelCount++;

	mEndIfLabels.push(endIfLabelId);

	if (resultTypeId == boolVectorTypeId)
	{
		uint32_t resultId0 = PushCompositeExtract(resultId, 0);
		uint32_t resultId1 = PushCompositeExtract(resultId, 1);
		uint32_t resultId2 = PushCompositeExtract(resultId, 2);
		uint32_t resultId3 = PushCompositeExtract(resultId, 3);

		uint32_t result01Id = GetNextId();
		Push(spv::OpLogicalAnd, boolTypeId, result01Id, resultId0, resultId1);

		uint32_t result23Id = GetNextId();
		Push(spv::OpLogicalAnd, boolTypeId, result23Id, resultId2, resultId3);

		uint32_t result0123Id = GetNextId();
		Push(spv::OpLogicalAnd, boolTypeId, result0123Id, result01Id, result23Id);

		Push(spv::OpSelectionMerge, endIfLabelId, 0);
		Push(spv::OpBranchConditional, result0123Id, trueLabelId, falseLabelId);
	}
	else
	{
		Push(spv::OpSelectionMerge, endIfLabelId, 0);
		Push(spv::OpBranchConditional, resultId, trueLabelId, falseLabelId);
	}
	Push(spv::OpLabel, trueLabelId);
	PrintTokenInformation("IFC", argumentToken1, argumentToken2);
}

void ShaderConverter::Process_IF()
{
	TypeDescription typeDescription;
	spv::Op dataType;
	uint32_t dataTypeId;
	uint32_t argumentId1;
	uint32_t resultId;
	uint32_t trueLabelId;
	uint32_t falseLabelId;
	uint32_t endIfLabelId;

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);

	typeDescription = GetTypeByRegister(argumentToken1); //use argument type because result type may not be known.
	mIdTypePairs[mNextId] = typeDescription; //snag next id before increment.

	dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	dataTypeId = GetSpirVTypeId(typeDescription);
	argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);
	resultId = argumentId1;

	TypeDescription labelType;
	labelType.PrimaryType = spv::OpLabel;

	trueLabelId = GetNextId();
	falseLabelId = GetNextId();
	endIfLabelId = GetNextId();

	mIdTypePairs[trueLabelId] = labelType;
	mIdTypePairs[falseLabelId] = labelType;
	mIdTypePairs[endIfLabelId] = labelType;

	mFalseLabels.push(falseLabelId);
	mFalseLabelCount++;

	mEndIfLabels.push(endIfLabelId);

	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpSelectionMerge, endIfLabelId, 0);
		Push(spv::OpBranchConditional, resultId, trueLabelId, falseLabelId);
		Push(spv::OpLabel, trueLabelId);
		break;
	default:
		Log(warning) << "Process_IF - Unsupported data type " << dataType << std::endl;
		break;
	}

	PrintTokenInformation("IF", argumentToken1);
}

void ShaderConverter::Process_ELSE()
{
	uint32_t falseLabelId = mFalseLabels.top();
	uint32_t endIfLabelId = mEndIfLabels.top();

	mFalseLabels.pop();

	Push(spv::OpBranch, endIfLabelId);
	Push(spv::OpLabel, falseLabelId);
	PrintTokenInformation("ELSE");
}

void ShaderConverter::Process_ENDIF()
{
	if (mFalseLabels.size() == mFalseLabelCount)
	{
		Process_ELSE();
	}
	mFalseLabelCount--;

	uint32_t endIfLabelId = mEndIfLabels.top();
	mEndIfLabels.pop();

	Push(spv::OpBranch, endIfLabelId);
	Push(spv::OpLabel, endIfLabelId);
	PrintTokenInformation("ENDIF");
}

/*
This is what for loops look like in SPIR-V
------------------------------------------------

; OpPhi needs this to know when to initialize to 0.
%5 = OpLabel
OpBranch %11

; e.g. for ( i = 0 ; i <= 10 ;  ) {
%11 = OpLabel
%16 = OpPhi %int %int_0 %5 %24 %14
%19 = OpSLessThanEqual %bool %16 %int_10
OpLoopMerge %13 %14 None
OpBranchConditional %19 %14 %13

%14 = OpLabel
	(Logic goes here)
; i++
%24 = OpIAdd %int %16 %int_1
OpBranch %11

; }
%13 = OpLabel


   */

void ShaderConverter::Process_GENERIC_LOOP()
{
	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	spv::Op dataType = typeDescription.PrimaryType;
	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	TypeDescription integerType;
	integerType.PrimaryType = spv::OpTypeInt;
	uint32_t integerTypeId = GetSpirVTypeId(integerType);

	TypeDescription integerPointerType;
	integerPointerType.PrimaryType = spv::OpTypePointer;
	integerPointerType.SecondaryType = spv::OpTypeInt;
	uint32_t integerPointerTypeId = GetSpirVTypeId(integerPointerType);

	TypeDescription booleanType;
	booleanType.PrimaryType = spv::OpTypeBool;
	uint32_t booleanTypeId = GetSpirVTypeId(booleanType);

	TypeDescription labelType;
	labelType.PrimaryType = spv::OpLabel;
	uint32_t labelTypeId = GetSpirVTypeId(labelType);

	LoopIds loopIds;

	loopIds.CounterId = GetNextId();
	mIdTypePairs[loopIds.CounterId] = integerType;

	loopIds.VariableId = GetNextId();
	mIdTypePairs[loopIds.VariableId] = integerPointerType;

	loopIds.PreLoopId = GetNextId();
	mIdTypePairs[loopIds.PreLoopId] = labelType;

	loopIds.PreMergeLabelId = GetNextId();
	mIdTypePairs[loopIds.PreMergeLabelId] = labelType;

	loopIds.PreExecuteLabelId = GetNextId();
	mIdTypePairs[loopIds.PreExecuteLabelId] = labelType;

	loopIds.EndLabelId = GetNextId();
	mIdTypePairs[loopIds.EndLabelId] = labelType;

	mLoopIds.push(loopIds);

	// Start Loop Label
	Push(spv::OpLabel, loopIds.PreLoopId);
	Push(spv::OpBranch, loopIds.PreMergeLabelId);

	// Loop
	Push(spv::OpLabel, loopIds.PreMergeLabelId);
	Push(spv::OpPhi, loopIds.CounterId, mConstantIntegerIds[0], loopIds.PreLoopId, loopIds.VariableId, loopIds.PreExecuteLabelId);

	//Check condition
	uint32_t resultId = GetNextId();
	mIdTypePairs[resultId] = booleanType;
	Push(spv::OpSLessThanEqual, booleanTypeId, resultId, loopIds.CounterId, argumentId1);

	//Branch based on condition
	uint32_t conditionId = GetNextId();
	Push(spv::OpBranchConditional, conditionId, loopIds.PreExecuteLabelId, loopIds.EndLabelId);
	Push(spv::OpLabel, loopIds.PreExecuteLabelId);
}

// XXX: We probably want an assertion or something to check that the loop type
// matches the actual loop used so that ENDREP cannot close ENDLOOP.
void ShaderConverter::Process_GENERIC_ENDLOOP()
{
	TypeDescription integerType;
	integerType.PrimaryType = spv::OpTypeInt;
	uint32_t integerTypeId = GetSpirVTypeId(integerType);

	LoopIds loopIds = mLoopIds.top();
	mLoopIds.pop();

	//increment counter
	uint32_t resultId = GetNextId();
	mIdTypePairs[resultId] = integerType;
	Push(spv::OpIAdd, integerTypeId, loopIds.VariableId, loopIds.CounterId, mConstantIntegerIds[1]);

	//End loop
	Push(spv::OpBranch, loopIds.PreMergeLabelId);
	Push(spv::OpLabel, loopIds.EndLabelId);
}

void ShaderConverter::Process_LOOP()
{
	Process_GENERIC_LOOP();

	LoopIds &loopIds = mLoopIds.top();
	loopIds.LoopType = LOOP;

	PrintTokenInformation("LOOP");
}

void ShaderConverter::Process_ENDLOOP()
{
	Process_GENERIC_ENDLOOP();
	PrintTokenInformation("ENDLOOP");
}

void ShaderConverter::Process_REP()
{
	Process_GENERIC_LOOP();

	LoopIds &loopIds = mLoopIds.top();
	loopIds.LoopType = REP;

	PrintTokenInformation("REP");
}

void ShaderConverter::Process_ENDREP()
{
	Process_GENERIC_ENDLOOP();
	PrintTokenInformation("ENDREP");
}

void ShaderConverter::Process_BREAK()
{
	LoopIds loopIds = mLoopIds.top();

	Push(spv::OpBranch, loopIds.EndLabelId);

	PrintTokenInformation("BREAK");
}

void ShaderConverter::Process_NRM()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Normalize, argumentId1);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Normalize, argumentId1);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Normalize, argumentId1);
		break;
	default:
		Log(warning) << "Process_NRM - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("NRM", resultToken, argumentToken1);
}

void ShaderConverter::Process_MOV()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	//Push(spv::OpCopyObject, dataTypeId, resultId, argumentId1);

	resultId = argumentId1;

	mIdTypePairs[resultId] = typeDescription;

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("MOV", resultToken, resultId, argumentToken1, argumentId1);
}

void ShaderConverter::Process_MOVA()
{
	TypeDescription typeDescription;
	spv::Op dataType;
	uint32_t dataTypeId;
	uint32_t argumentId1;
	uint32_t resultId;

	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);

	typeDescription = GetTypeByRegister(argumentToken1); //use argument type because result type may not be known.
	mIdTypePairs[mNextId] = typeDescription; //snag next id before increment.

	dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = spv::OpTypeInt;
	}
	else
	{
		typeDescription.PrimaryType = spv::OpTypeInt;
	}

	dataTypeId = GetSpirVTypeId(typeDescription);
	argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);
	resultId = GetNextId();

	Push(spv::OpConvertFToS, dataTypeId, resultId, argumentId1);

	mIdTypePairs[resultId] = typeDescription;

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("MOVA", resultToken, argumentToken1);
}

void ShaderConverter::Process_RSQ()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	/*
	The operand x must be a scalar or vector whose component type is floating-point.

	Result Type and the type of x must be the same type. Results are computed per component.
	*/
	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeFloat:
		PushInverseSqrt(dataTypeId, resultId, argumentId1);
		break;
	default:
		Log(warning) << "Process_RSQ - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("RSQ", resultToken, argumentToken1);
}

void ShaderConverter::Process_RCP()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeFloat:
		if (typeDescription.PrimaryType == spv::OpTypeVector)
		{
			Push(spv::OpFDiv, dataTypeId, resultId, argumentId1, m1vfId);
		}
		else
		{
			Push(spv::OpFDiv, dataTypeId, resultId, argumentId1, m1fId);
		}
		break;
	case spv::OpTypeInt:
		if (typeDescription.PrimaryType == spv::OpTypeVector)
		{
			Log(warning) << "Process_RCP - Unsupported data type vector of int." << std::endl;
		}
		else
		{
			Push(spv::OpSDiv, dataTypeId, resultId, argumentId1, mConstantIntegerIds[1]);
		}
		break;
	default:
		Log(warning) << "Process_RCP - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("RCP", resultToken, argumentToken1);
}

void ShaderConverter::Process_TEXBEM()
{
	TypeDescription floatVector4Type;
	floatVector4Type.PrimaryType = spv::OpTypeVector;
	floatVector4Type.SecondaryType = spv::OpTypeFloat;
	floatVector4Type.ComponentCount = 4;
	uint32_t floatVector4TypeId = GetSpirVTypeId(floatVector4Type);

	TypeDescription floatVector2Type;
	floatVector2Type.PrimaryType = spv::OpTypeVector;
	floatVector2Type.SecondaryType = spv::OpTypeFloat;
	floatVector2Type.ComponentCount = 2;
	uint32_t floatVector2TypeId = GetSpirVTypeId(floatVector2Type);

	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	TypeDescription floatPointerType;
	floatPointerType.PrimaryType = spv::OpTypePointer;
	floatPointerType.SecondaryType = spv::OpTypeFloat;
	floatPointerType.StorageClass = spv::StorageClassUniform;
	uint32_t floatPointerTypeId = GetSpirVTypeId(floatPointerType);

	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = resultToken;
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t textureCoordinatesMId = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_2);
	uint32_t samplerMId = GetSwizzledId(argumentToken1, GIVE_ME_SAMPLER);
	uint32_t registerNumberM = argumentToken1.DestinationParameterToken.RegisterNumber;

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t rgbaNId = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4); //users are required to use tex first so the rgba result id should already be on file.

	//Grab the environment bump map components.
	uint32_t textureStageMId = PushAccessChain(mTextureStagesId, registerNumberM);

	uint32_t bumpEnvironmentMatrix00PointerMId = PushAccessChain(textureStageMId, 13);
	uint32_t bumpEnvironmentMatrix00MId = PushLoad(bumpEnvironmentMatrix00PointerMId);

	uint32_t bumpEnvironmentMatrix10PointerMId = PushAccessChain(textureStageMId, 15);
	uint32_t bumpEnvironmentMatrix10MId = PushLoad(bumpEnvironmentMatrix10PointerMId);

	uint32_t bumpEnvironmentMatrix01PointerMId = PushAccessChain(textureStageMId, 14);
	uint32_t bumpEnvironmentMatrix01MId = PushLoad(bumpEnvironmentMatrix01PointerMId);

	uint32_t bumpEnvironmentMatrix11PointerMId = PushAccessChain(textureStageMId, 16);
	uint32_t bumpEnvironmentMatrix11MId = PushLoad(bumpEnvironmentMatrix11PointerMId);

	//grab u and v from m.
	uint32_t uMId = PushCompositeExtract(textureCoordinatesMId, 0);
	uint32_t vMId = PushCompositeExtract(textureCoordinatesMId, 1);

	//grab r and g from n
	uint32_t rNId = PushCompositeExtract(rgbaNId, 0);
	uint32_t gNId = PushCompositeExtract(rgbaNId, 1);

	//calculate u'
	uint32_t r00Id = GetNextId();
	mIdTypePairs[r00Id] = floatType;
	Push(spv::OpFMul, floatTypeId, r00Id, bumpEnvironmentMatrix00MId, rNId);

	uint32_t g10Id = GetNextId();
	mIdTypePairs[g10Id] = floatType;
	Push(spv::OpFMul, floatTypeId, g10Id, bumpEnvironmentMatrix10MId, gNId);

	uint32_t rg0010Id = GetNextId();
	mIdTypePairs[rg0010Id] = floatType;
	Push(spv::OpFAdd, floatTypeId, rg0010Id, r00Id, g10Id);

	uint32_t uId = GetNextId();
	mIdTypePairs[uId] = floatType;
	Push(spv::OpFAdd, floatTypeId, uId, uMId, rg0010Id);

	//calculate v'
	uint32_t r01Id = GetNextId();
	mIdTypePairs[r01Id] = floatType;
	Push(spv::OpFMul, floatTypeId, r01Id, bumpEnvironmentMatrix01MId, rNId);

	uint32_t g11Id = GetNextId();
	mIdTypePairs[g11Id] = floatType;
	Push(spv::OpFMul, floatTypeId, g11Id, bumpEnvironmentMatrix11MId, gNId);

	uint32_t rg0111Id = GetNextId();
	mIdTypePairs[rg0111Id] = floatType;
	Push(spv::OpFAdd, floatTypeId, rg0111Id, r01Id, g11Id);

	uint32_t vId = GetNextId();
	mIdTypePairs[vId] = floatType;
	Push(spv::OpFAdd, floatTypeId, vId, vMId, rg0111Id);

	//glue u and v back together.
	uint32_t uvId = GetNextId();
	mIdTypePairs[uvId] = floatVector2Type;
	Push(spv::OpCompositeConstruct, floatVector2TypeId, uvId, uId, vId);

	//Sample the image
	mIdTypePairs[resultId] = floatVector4Type;
	Push(spv::OpImageSampleImplicitLod, floatVector4TypeId, resultId, samplerMId, uvId);

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("TEXBEM", resultToken, argumentToken1, argumentToken2);
}

/*
If x, y, or z are less than zero then kill the pixel.
*/
void ShaderConverter::Process_TEXKILL()
{
	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	TypeDescription boolType;
	boolType.PrimaryType = spv::OpTypeBool;
	uint32_t boolTypeId = GetSpirVTypeId(boolType);

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	//X
	uint32_t xId = PushCompositeExtract(argumentId1, 0);

	uint32_t xConditionalId = GetNextId();
	uint32_t xTrueLabelId = GetNextId();
	uint32_t xFalseLabelId = GetNextId();

	mIdTypePairs[xConditionalId] = boolType;

	Push(spv::OpFOrdLessThan, boolTypeId, xConditionalId, xId, m0fId);
	Push(spv::OpSelectionMerge, xFalseLabelId, 0);
	Push(spv::OpBranchConditional, xConditionalId, xTrueLabelId, xFalseLabelId);
	Push(spv::OpLabel, xTrueLabelId);
	Push(spv::OpKill);
	Push(spv::OpLabel, xFalseLabelId);

	//Y
	uint32_t yId = PushCompositeExtract(argumentId1, 1);

	uint32_t yConditionalId = GetNextId();
	uint32_t yTrueLabelId = GetNextId();
	uint32_t yFalseLabelId = GetNextId();

	mIdTypePairs[yConditionalId] = boolType;

	Push(spv::OpFOrdLessThan, boolTypeId, yConditionalId, yId, m0fId);
	Push(spv::OpSelectionMerge, yFalseLabelId, 0);
	Push(spv::OpBranchConditional, yConditionalId, yTrueLabelId, yFalseLabelId);
	Push(spv::OpLabel, yTrueLabelId);
	Push(spv::OpKill);
	Push(spv::OpLabel, yFalseLabelId);

	//Z
	uint32_t zId = PushCompositeExtract(argumentId1, 2);

	uint32_t zConditionalId = GetNextId();
	uint32_t zTrueLabelId = GetNextId();
	uint32_t zFalseLabelId = GetNextId();

	mIdTypePairs[zConditionalId] = boolType;

	Push(spv::OpFOrdLessThan, boolTypeId, zConditionalId, zId, m0fId);
	Push(spv::OpSelectionMerge, zFalseLabelId, 0);
	Push(spv::OpBranchConditional, zConditionalId, zTrueLabelId, zFalseLabelId);
	Push(spv::OpLabel, zTrueLabelId);
	Push(spv::OpKill);
	Push(spv::OpLabel, zFalseLabelId);

	PrintTokenInformation("TEXKILL", argumentToken1);
}

void ShaderConverter::Process_GenericBinaryOperation(const char* tokenName, spv::Op floatOp, spv::Op integerOp)
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	//Grab type info if there is any.
	TypeDescription argumentType1 = mIdTypePairs[argumentId1];
	TypeDescription argumentType2 = mIdTypePairs[argumentId2];
	TypeDescription typeDescription;


	//Shift the result type so we get a register instead of a pointer as the output type.
	if (argumentType1.PrimaryType == spv::OpTypePointer)
	{

		argumentType1.PrimaryType = argumentType1.SecondaryType;
		argumentType1.SecondaryType = argumentType1.TernaryType;
		argumentType1.TernaryType = spv::OpTypeVoid;
	}

	if (argumentType2.PrimaryType == spv::OpTypePointer)
	{
		argumentType2.PrimaryType = argumentType2.SecondaryType;
		argumentType2.SecondaryType = argumentType2.TernaryType;
		argumentType2.TernaryType = spv::OpTypeVoid;
	}

	//Figure out which OpCode to used based on the argument types.
	//Unforunately DXBC doesn't care about argument types for Addtiply but SpirV does.
	if (argumentType1.PrimaryType == spv::OpTypeFloat && argumentType2.PrimaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		Push(floatOp, dataTypeId, resultId, argumentId1, argumentId2);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeFloat && argumentType2.PrimaryType == spv::OpTypeInt)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

		uint32_t convertedId = GetNextId();
		mIdTypePairs[convertedId] = typeDescription;
		Push(spv::OpConvertUToF, dataTypeId, convertedId, argumentId2);

		mIdTypePairs[resultId] = typeDescription;
		Push(floatOp, dataTypeId, resultId, argumentId1, convertedId);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeInt && argumentType2.PrimaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType2;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

		uint32_t convertedId = GetNextId();
		mIdTypePairs[convertedId] = typeDescription;
		Push(spv::OpConvertUToF, dataTypeId, convertedId, argumentId1);

		mIdTypePairs[resultId] = typeDescription;
		Push(floatOp, dataTypeId, resultId, convertedId, argumentId2);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeVector
		&& argumentType1.SecondaryType == spv::OpTypeFloat && argumentType2.SecondaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		Push(floatOp, dataTypeId, resultId, argumentId1, argumentId2);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeVector
		&& argumentType1.SecondaryType == spv::OpTypeInt && argumentType2.SecondaryType == spv::OpTypeInt)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		Push(integerOp, dataTypeId, resultId, argumentId1, argumentId2);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeVector
		&& argumentType1.SecondaryType == spv::OpTypeFloat && argumentType2.SecondaryType == spv::OpTypeInt)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

		uint32_t convertedId = GetNextId();
		mIdTypePairs[convertedId] = typeDescription;
		Push(spv::OpConvertUToF, dataTypeId, convertedId, argumentId2);

		mIdTypePairs[resultId] = typeDescription;
		Push(floatOp, dataTypeId, resultId, argumentId1, convertedId);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeVector
		&& argumentType1.SecondaryType == spv::OpTypeInt && argumentType2.SecondaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType2;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

		uint32_t convertedId = GetNextId();
		mIdTypePairs[convertedId] = typeDescription;
		Push(spv::OpConvertUToF, dataTypeId, convertedId, argumentId1);

		mIdTypePairs[resultId] = typeDescription;
		Push(floatOp, dataTypeId, resultId, convertedId, argumentId2);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeFloat)
	{
		if (argumentRegisterType1 == D3DSPR_CONST)
		{
			uint32_t argumentTypeId2 = GetSpirVTypeId(argumentType2);
			uint32_t xId = PushCompositeExtract(argumentId1, 0);

			uint32_t tempId = GetNextId();
			mIdTypePairs[tempId] = argumentType2;
			Push(floatOp, argumentTypeId2, tempId, xId, argumentId2);

			typeDescription = argumentType1;
			uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
			mIdTypePairs[resultId] = typeDescription;

			switch (typeDescription.ComponentCount)
			{
			case 2:
				Push(spv::OpCompositeConstruct, dataTypeId, resultId, tempId, tempId);
				break;
			case 3:
				Push(spv::OpCompositeConstruct, dataTypeId, resultId, tempId, tempId, tempId);
				break;
			case 4:
				Push(spv::OpCompositeConstruct, dataTypeId, resultId, tempId, tempId, tempId, tempId);
				break;
			default:
				break;
			}
		}
		else
		{
			typeDescription = argumentType1;
			uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
			mIdTypePairs[resultId] = typeDescription;

			uint32_t compositeId = GetNextId();

			switch (typeDescription.ComponentCount)
			{
			case 2:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId2, argumentId2);
				break;
			case 3:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId2, argumentId2, argumentId2);
				break;
			case 4:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId2, argumentId2, argumentId2, argumentId2);
				break;
			default:
				break;
			}

			Push(floatOp, dataTypeId, resultId, argumentId1, compositeId);
		}
	}
	else if (argumentType1.PrimaryType == spv::OpTypeFloat && argumentType2.PrimaryType == spv::OpTypeVector)
	{
		if (argumentRegisterType2 == D3DSPR_CONST)
		{
			uint32_t argumentTypeId1 = GetSpirVTypeId(argumentType1);
			uint32_t xId = PushCompositeExtract(argumentId2, 0);

			uint32_t tempId = GetNextId();
			mIdTypePairs[tempId] = argumentType1;
			Push(floatOp, argumentTypeId1, tempId, xId, argumentId1);

			typeDescription = argumentType2;
			uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
			mIdTypePairs[resultId] = typeDescription;

			switch (typeDescription.ComponentCount)
			{
			case 2:
				Push(spv::OpCompositeConstruct, dataTypeId, resultId, tempId, tempId);
				break;
			case 3:
				Push(spv::OpCompositeConstruct, dataTypeId, resultId, tempId, tempId, tempId);
				break;
			case 4:
				Push(spv::OpCompositeConstruct, dataTypeId, resultId, tempId, tempId, tempId, tempId);
				break;
			default:
				break;
			}
		}
		else
		{
			typeDescription = argumentType2;
			uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
			mIdTypePairs[resultId] = typeDescription;

			uint32_t compositeId = GetNextId();

			switch (typeDescription.ComponentCount)
			{
			case 2:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId1, argumentId1);
				break;
			case 3:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId1, argumentId1, argumentId1);
				break;
			case 4:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId1, argumentId1, argumentId1, argumentId1);
				break;
			default:
				break;
			}

			Push(floatOp, dataTypeId, resultId, argumentId2, compositeId);
		}

	}
	else if (argumentType1.PrimaryType == spv::OpTypeInt && argumentType2.PrimaryType == spv::OpTypeInt)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		Push(integerOp, dataTypeId, resultId, argumentId1, argumentId2);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeBool && argumentType2.PrimaryType == spv::OpTypeBool)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		Push(integerOp, dataTypeId, resultId, argumentId1, argumentId2);
	}
	else
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		Push(floatOp, dataTypeId, resultId, argumentId1, argumentId2);

		Log(warning) << "Process_" << tokenName << " - Unsupported data types " << argumentType1.PrimaryType << " " << argumentType2.PrimaryType << std::endl;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation(tokenName, resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_DST()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Distance, argumentId1, argumentId2);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Distance, argumentId1, argumentId2);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Distance, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_DST - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("DST", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_CRS()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Cross, argumentId1, argumentId2);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Cross, argumentId1, argumentId2);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Cross, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_CRS - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("CRS", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_POW()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Pow, argumentId1, argumentId2);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Pow, argumentId1, argumentId2);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Pow, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_POW - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("POW", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_MUL()
{
	Process_GenericBinaryOperation("MUL", spv::OpFMul, spv::OpIMul);
}

void ShaderConverter::Process_EXP()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Exp2, argumentId1);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Exp2, argumentId1);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Exp2, argumentId1);
		break;
	default:
		Log(warning) << "Process_EXP - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("EXP", resultToken, argumentToken1);
}

void ShaderConverter::Process_EXPP()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	//Token argumentToken2 = GetNextToken();
	//_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	//uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Exp2, argumentId1);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Exp2, argumentId1);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Exp2, argumentId1);
		break;
	default:
		Log(warning) << "Process_EXPP - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("EXPP", resultToken, argumentToken1);
}

void ShaderConverter::Process_LOG()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Log2, argumentId1);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Log2, argumentId1);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Log2, argumentId1);
		break;
	default:
		Log(warning) << "Process_LOG - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("LOG", resultToken, argumentToken1);
}

void ShaderConverter::Process_LOGP()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Log2, argumentId1);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Log2, argumentId1);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Log2, argumentId1);
		break;
	default:
		Log(warning) << "Process_LOGP - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("LOGP", resultToken, argumentToken1);
}

void ShaderConverter::Process_FRC()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	//Grab type info if there is any.
	TypeDescription argumentType1 = mIdTypePairs[argumentId1];
	TypeDescription typeDescription;

	//Shift the result type so we get a register instead of a pointer as the output type.
	if (argumentType1.PrimaryType == spv::OpTypePointer)
	{

		argumentType1.PrimaryType = argumentType1.SecondaryType;
		argumentType1.SecondaryType = argumentType1.TernaryType;
		argumentType1.TernaryType = spv::OpTypeVoid;
	}

	typeDescription = argumentType1;
	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	//Create variable for whole number output. (not used by DXBC)
	uint32_t argumentId2 = GetNextId();

	TypeDescription wholePointerType;
	wholePointerType.PrimaryType = spv::OpTypePointer;
	wholePointerType.SecondaryType = typeDescription.PrimaryType;
	wholePointerType.TernaryType = typeDescription.SecondaryType;
	wholePointerType.ComponentCount = typeDescription.ComponentCount;
	uint32_t wholePointerTypeId = GetSpirVTypeId(wholePointerType);

	mTypeInstructions.push_back(Pack(4, spv::OpVariable)); //size,Type
	mTypeInstructions.push_back(wholePointerTypeId); //ResultType (Id) Must be OpTypePointer with the pointer's type being what you care about.
	mTypeInstructions.push_back(argumentId2); //Result (Id)
	mTypeInstructions.push_back(spv::StorageClassPrivate); //Storage Class
	mIdTypePairs[argumentId2] = wholePointerType;

	//Now generate the real FCR instruction.
	mIdTypePairs[resultId] = typeDescription;

	Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450Modf, argumentId1, argumentId2);

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("FRC", resultToken, argumentToken1);
}


void ShaderConverter::Process_ABS()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450SAbs, argumentId1);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450SAbs, argumentId1);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450FAbs, argumentId1);
		break;
	default:
		Log(warning) << "Process_ABS - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("ABS", resultToken, argumentToken1);
}

void ShaderConverter::Process_ADD()
{
	Process_GenericBinaryOperation("ADD", spv::OpFAdd, spv::OpIAdd);
}

void ShaderConverter::Process_SUB()
{
	Process_GenericBinaryOperation("SUB", spv::OpFSub, spv::OpISub);
}

void ShaderConverter::Process_MIN()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450UMin, argumentId1, argumentId2);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450SMin, argumentId1, argumentId2);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450FMin, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_MIN - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("MIN", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_MAX()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450UMax, argumentId1, argumentId2);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450SMax, argumentId1, argumentId2);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450FMax, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_MAX - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("MAX", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_SGE()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	TypeDescription booleanType;
	booleanType.PrimaryType = spv::OpTypeBool;
	uint32_t booleanTypeId = GetSpirVTypeId(booleanType);

	mIdTypePairs[resultId] = booleanType;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpSGreaterThanEqual, booleanTypeId, resultId, argumentId1, argumentId2);
		break;
	case spv::OpTypeInt:
		Push(spv::OpSGreaterThanEqual, booleanTypeId, resultId, argumentId1, argumentId2);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpFUnordGreaterThanEqual, booleanTypeId, resultId, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_SGE - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("SGE", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_SLT()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	TypeDescription booleanType;
	booleanType.PrimaryType = spv::OpTypeBool;
	uint32_t booleanTypeId = GetSpirVTypeId(booleanType);

	mIdTypePairs[resultId] = booleanType;
	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpSLessThan, booleanTypeId, resultId, argumentId1, argumentId2);
		break;
	case spv::OpTypeInt:
		Push(spv::OpSLessThan, booleanTypeId, resultId, argumentId1, argumentId2);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpFUnordLessThan, booleanTypeId, resultId, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_SLT - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("SLT", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_DP3()
{
	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	//DP output is float even though input is a vec.
	mIdTypePairs[resultId] = floatType;
	Push(spv::OpDot, floatTypeId, resultId, argumentId1, argumentId2);

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("DP3", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_DP4()
{
	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	//DP output is float even though input is a vec.
	mIdTypePairs[resultId] = floatType;
	Push(spv::OpDot, floatTypeId, resultId, argumentId1, argumentId2);

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("DP4", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_TEX()
{
	TypeDescription vectorType;
	vectorType.PrimaryType = spv::OpTypeVector;
	vectorType.SecondaryType = spv::OpTypeFloat;
	vectorType.ComponentCount = 4;
	uint32_t vectorTypeId = GetSpirVTypeId(vectorType);

	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = (mMajorVersion > 1 || mMinorVersion >= 4) ? GetNextToken() : resultToken;
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_2);

	Token argumentToken2 = (mMajorVersion > 1) ? GetNextToken() : argumentToken1;
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_SAMPLER);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	//TEX output is vec4 even though input is a vec2 and sampler.
	mIdTypePairs[resultId] = vectorType;
	Push(spv::OpImageSampleImplicitLod, vectorTypeId, resultId, argumentId2, argumentId1);

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("TEX", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_TEXCOORD()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = (mMajorVersion > 1 || mMinorVersion >= 4) ? GetNextToken() : resultToken;
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	//Push(spv::OpCopyObject, dataTypeId, resultId, argumentId1);

	resultId = argumentId1;

	mIdTypePairs[resultId] = typeDescription;

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("TEXCOORD", resultToken, argumentToken1);
}

void ShaderConverter::Process_M4x4()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_MATRIX_4X4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeFloat:
		Push(spv::OpVectorTimesMatrix, dataTypeId, resultId, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_M4x4 - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("M4x4", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_M4x3()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_MATRIX_4X4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeFloat:
		Push(spv::OpVectorTimesMatrix, dataTypeId, resultId, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_M4x3 - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("M4x3", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_M3x4()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_3);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_MATRIX_3X3);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeFloat:
		Push(spv::OpVectorTimesMatrix, dataTypeId, resultId, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_M3x4 - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("M3x4", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_M3x3()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_3);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_MATRIX_3X3);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeFloat:
		Push(spv::OpVectorTimesMatrix, dataTypeId, resultId, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_M3x3 - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("M3x3", resultToken, argumentToken1, argumentToken2);
}

void ShaderConverter::Process_M3x2()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_3);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_MATRIX_3X3);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeFloat:
		Push(spv::OpVectorTimesMatrix, dataTypeId, resultId, argumentId1, argumentId2);
		break;
	default:
		Log(warning) << "Process_M3x2 - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("M3x2", resultToken, argumentToken1, argumentToken2);
}

/*
I don't know why SPIR-V doesn't have a MAD instruction.
*/
void ShaderConverter::Process_MAD()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();
	uint32_t resultId2 = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	Token argumentToken3 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType3 = GetRegisterType(argumentToken3.i);
	uint32_t argumentId3 = GetSwizzledId(argumentToken3, GIVE_ME_VECTOR_4);

	//Grab type info if there is any.
	TypeDescription argumentType1 = mIdTypePairs[argumentId1];
	TypeDescription argumentType2 = mIdTypePairs[argumentId2];
	TypeDescription argumentType3 = mIdTypePairs[argumentId3];
	TypeDescription typeDescription;

	//Shift the result type so we get a register instead of a pointer as the output type.
	if (argumentType1.PrimaryType == spv::OpTypePointer)
	{

		argumentType1.PrimaryType = argumentType1.SecondaryType;
		argumentType1.SecondaryType = argumentType1.TernaryType;
		argumentType1.TernaryType = spv::OpTypeVoid;
	}

	if (argumentType2.PrimaryType == spv::OpTypePointer)
	{
		argumentType2.PrimaryType = argumentType2.SecondaryType;
		argumentType2.SecondaryType = argumentType2.TernaryType;
		argumentType2.TernaryType = spv::OpTypeVoid;
	}

	if (argumentType3.PrimaryType == spv::OpTypePointer)
	{
		argumentType3.PrimaryType = argumentType3.SecondaryType;
		argumentType3.SecondaryType = argumentType3.TernaryType;
		argumentType3.TernaryType = spv::OpTypeVoid;
	}

	//Figure out which OpCode to used based on the argument types.
	//Unforunately DXBC doesn't care about argument types for multiply but SpirV does.
	if (argumentType1.PrimaryType == spv::OpTypeFloat && argumentType2.PrimaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		mIdTypePairs[resultId2] = typeDescription;
		Push(spv::OpFMul, dataTypeId, resultId, argumentId1, argumentId2);

		if (argumentType3.PrimaryType == spv::OpTypeFloat)
		{
			Push(spv::OpFAdd, dataTypeId, resultId2, resultId, argumentId3);
		}
		else
		{
			typeDescription = argumentType3;
			mIdTypePairs[resultId2] = typeDescription;

			uint32_t compositeId = GetNextId();

			switch (typeDescription.ComponentCount)
			{
			case 2:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, resultId, resultId);
				break;
			case 3:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, resultId, resultId, resultId);
				break;
			case 4:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, resultId, resultId, resultId, resultId);
				break;
			default:
				break;
			}

			Push(spv::OpFAdd, dataTypeId, resultId2, compositeId, argumentId3);
		}
	}
	else if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeVector
		&& argumentType1.SecondaryType == spv::OpTypeFloat && argumentType2.SecondaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		mIdTypePairs[resultId2] = typeDescription;
		Push(spv::OpFMul, dataTypeId, resultId, argumentId1, argumentId2);

		if (argumentType3.PrimaryType == spv::OpTypeVector)
		{
			Push(spv::OpFAdd, dataTypeId, resultId2, resultId, argumentId3);
		}
		else
		{
			uint32_t compositeId = GetNextId();

			switch (typeDescription.ComponentCount)
			{
			case 2:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId3, argumentId3);
				break;
			case 3:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId3, argumentId3, argumentId3);
				break;
			case 4:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId3, argumentId3, argumentId3, argumentId3);
				break;
			default:
				break;
			}

			Push(spv::OpFAdd, dataTypeId, resultId2, resultId, compositeId);
		}
	}
	else if (argumentType1.PrimaryType == spv::OpTypeMatrix && argumentType2.PrimaryType == spv::OpTypeMatrix)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		mIdTypePairs[resultId2] = typeDescription;
		Push(spv::OpMatrixTimesMatrix, dataTypeId, resultId, argumentId1, argumentId2);
		Push(spv::OpFAdd, dataTypeId, resultId2, resultId, argumentId3); //TODO: revisit
	}
	else if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		mIdTypePairs[resultId2] = typeDescription;
		Push(spv::OpVectorTimesScalar, dataTypeId, resultId, argumentId1, argumentId2);

		if (argumentType3.PrimaryType == spv::OpTypeVector)
		{
			Push(spv::OpFAdd, dataTypeId, resultId2, resultId, argumentId3);
		}
		else
		{
			uint32_t compositeId = GetNextId();

			switch (typeDescription.ComponentCount)
			{
			case 2:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId3, argumentId3);
				break;
			case 3:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId3, argumentId3, argumentId3);
				break;
			case 4:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId3, argumentId3, argumentId3, argumentId3);
				break;
			default:
				break;
			}

			Push(spv::OpFAdd, dataTypeId, resultId2, resultId, compositeId);
		}
	}
	else if (argumentType1.PrimaryType == spv::OpTypeFloat && argumentType2.PrimaryType == spv::OpTypeVector)
	{
		typeDescription = argumentType2;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		mIdTypePairs[resultId2] = typeDescription;
		Push(spv::OpVectorTimesScalar, dataTypeId, resultId, argumentId2, argumentId1);

		if (argumentType3.PrimaryType == spv::OpTypeVector)
		{
			Push(spv::OpFAdd, dataTypeId, resultId2, resultId, argumentId3);
		}
		else
		{
			uint32_t compositeId = GetNextId();

			switch (typeDescription.ComponentCount)
			{
			case 2:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId3, argumentId3);
				break;
			case 3:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId3, argumentId3, argumentId3);
				break;
			case 4:
				Push(spv::OpCompositeConstruct, dataTypeId, compositeId, argumentId3, argumentId3, argumentId3, argumentId3);
				break;
			default:
				break;
			}

			Push(spv::OpFAdd, dataTypeId, resultId2, resultId, compositeId);
		}
	}
	else if (argumentType1.PrimaryType == spv::OpTypeMatrix && argumentType2.PrimaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		mIdTypePairs[resultId2] = typeDescription;
		Push(spv::OpMatrixTimesScalar, dataTypeId, resultId, argumentId1, argumentId2);
		Push(spv::OpFAdd, dataTypeId, resultId2, resultId, argumentId3); //TODO: revisit
	}
	else if (argumentType1.PrimaryType == spv::OpTypeInt && argumentType2.PrimaryType == spv::OpTypeInt)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		mIdTypePairs[resultId2] = typeDescription;
		Push(spv::OpIMul, dataTypeId, resultId, argumentId1, argumentId2);
		Push(spv::OpIAdd, dataTypeId, resultId2, resultId, argumentId3);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeBool && argumentType2.PrimaryType == spv::OpTypeBool)
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		mIdTypePairs[resultId2] = typeDescription;
		Push(spv::OpIMul, dataTypeId, resultId, argumentId1, argumentId2);
		Push(spv::OpIAdd, dataTypeId, resultId2, resultId, argumentId3);
	}
	else
	{
		typeDescription = argumentType1;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId] = typeDescription;
		mIdTypePairs[resultId2] = typeDescription;
		Push(spv::OpFMul, dataTypeId, resultId, argumentId1, argumentId2);
		Push(spv::OpFAdd, dataTypeId, resultId2, resultId, argumentId3);

		Log(warning) << "Process_MAD - Unsupported data types " << argumentType1.PrimaryType << " " << argumentType2.PrimaryType << " " << argumentType3.PrimaryType << std::endl;
	}

	resultId2 = ApplyWriteMask(resultToken, resultId2);

	PrintTokenInformation("MAD", resultToken, argumentToken1, argumentToken2, argumentToken3);
}

void ShaderConverter::Process_CMP()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();
	uint32_t resultId2 = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	Token argumentToken3 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType3 = GetRegisterType(argumentToken3.i);
	uint32_t argumentId3 = GetSwizzledId(argumentToken3, GIVE_ME_VECTOR_4);

	//Grab type info if there is any.
	TypeDescription argumentType1 = mIdTypePairs[argumentId1];
	TypeDescription argumentType2 = mIdTypePairs[argumentId2];
	TypeDescription argumentType3 = mIdTypePairs[argumentId3];
	TypeDescription typeDescription;

	//Shift the result type so we get a register instead of a pointer as the output type.
	if (argumentType1.PrimaryType == spv::OpTypePointer)
	{

		argumentType1.PrimaryType = argumentType1.SecondaryType;
		argumentType1.SecondaryType = argumentType1.TernaryType;
		argumentType1.TernaryType = spv::OpTypeVoid;
	}

	if (argumentType2.PrimaryType == spv::OpTypePointer)
	{
		argumentType2.PrimaryType = argumentType2.SecondaryType;
		argumentType2.SecondaryType = argumentType2.TernaryType;
		argumentType2.TernaryType = spv::OpTypeVoid;
	}

	if (argumentType3.PrimaryType == spv::OpTypePointer)
	{
		argumentType3.PrimaryType = argumentType3.SecondaryType;
		argumentType3.SecondaryType = argumentType3.TernaryType;
		argumentType3.TernaryType = spv::OpTypeVoid;
	}

	TypeDescription booleanType;
	booleanType.PrimaryType = spv::OpTypeBool;
	uint32_t booleanTypeId = GetSpirVTypeId(booleanType);

	TypeDescription booleanVectorType;
	booleanVectorType.PrimaryType = spv::OpTypeVector;
	booleanVectorType.SecondaryType = spv::OpTypeBool;
	booleanVectorType.ComponentCount = 4;
	uint32_t booleanVectorTypeId = GetSpirVTypeId(booleanVectorType);

	if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeVector && argumentType3.PrimaryType == spv::OpTypeVector)
	{
		typeDescription = argumentType2;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId2] = typeDescription;


		mIdTypePairs[resultId] = booleanVectorType;
		Push(spv::OpFOrdGreaterThanEqual, booleanVectorTypeId, resultId, argumentId1, m0vfId);
		Push(spv::OpSelect, dataTypeId, resultId2, resultId, argumentId2, argumentId3);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeFloat && argumentType2.PrimaryType == spv::OpTypeFloat && argumentType3.PrimaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType2;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId2] = typeDescription;


		mIdTypePairs[resultId] = booleanType;
		Push(spv::OpFOrdGreaterThanEqual, booleanTypeId, resultId, argumentId1, m0fId);
		Push(spv::OpSelect, dataTypeId, resultId2, resultId, argumentId2, argumentId3);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeInt && argumentType2.PrimaryType == spv::OpTypeInt && argumentType3.PrimaryType == spv::OpTypeInt)
	{
		typeDescription = argumentType2;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId2] = typeDescription;


		mIdTypePairs[resultId] = booleanType;
		Push(spv::OpSGreaterThanEqual, booleanTypeId, resultId, argumentId1, mConstantIntegerIds[0]);
		Push(spv::OpSelect, dataTypeId, resultId2, resultId, argumentId2, argumentId3);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeFloat && argumentType2.PrimaryType == spv::OpTypeVector && argumentType3.PrimaryType == spv::OpTypeVector)
	{
		typeDescription = argumentType2;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId2] = typeDescription;


		mIdTypePairs[resultId] = booleanType;
		Push(spv::OpFOrdGreaterThanEqual, booleanTypeId, resultId, argumentId1, m0fId);

		//OpSelect requires the condition to have the same number of components because results are computed per component.
		uint32_t compositeId = GetNextId();
		mIdTypePairs[compositeId] = booleanVectorType;
		Push(spv::OpCompositeConstruct, booleanVectorTypeId, compositeId, resultId, resultId, resultId, resultId);

		Push(spv::OpSelect, dataTypeId, resultId2, compositeId, argumentId2, argumentId3);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeFloat && argumentType2.PrimaryType == spv::OpTypeFloat && argumentType3.PrimaryType == spv::OpTypeVector)
	{
		typeDescription = argumentType3;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId2] = typeDescription;

		mIdTypePairs[resultId] = booleanVectorType;
		Push(spv::OpFOrdGreaterThanEqual, booleanVectorTypeId, resultId, argumentId1, m0fId);

		uint32_t compositeId1 = GetNextId();
		mIdTypePairs[compositeId1] = booleanVectorType;
		Push(spv::OpCompositeConstruct, booleanVectorTypeId, compositeId1, resultId, resultId, resultId, resultId);

		uint32_t compositeId2 = GetNextId();
		mIdTypePairs[compositeId2] = typeDescription;
		Push(spv::OpCompositeConstruct, booleanVectorTypeId, compositeId2, argumentId2, argumentId2, argumentId2, argumentId2);

		Push(spv::OpSelect, dataTypeId, resultId2, compositeId1, compositeId2, argumentId3);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeFloat && argumentType3.PrimaryType == spv::OpTypeVector)
	{
		typeDescription = argumentType3;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId2] = typeDescription;

		mIdTypePairs[resultId] = booleanVectorType;
		Push(spv::OpFOrdGreaterThanEqual, booleanVectorTypeId, resultId, argumentId1, m0vfId);

		uint32_t compositeId1 = GetNextId();
		mIdTypePairs[compositeId1] = typeDescription;
		Push(spv::OpCompositeConstruct, dataTypeId, compositeId1, argumentId2, argumentId2, argumentId2, argumentId2);

		Push(spv::OpSelect, dataTypeId, resultId2, resultId, compositeId1, argumentId3);
	}
	else if (argumentType1.PrimaryType == spv::OpTypeVector && argumentType2.PrimaryType == spv::OpTypeVector && argumentType3.PrimaryType == spv::OpTypeFloat)
	{
		typeDescription = argumentType2;
		uint32_t dataTypeId = GetSpirVTypeId(typeDescription);
		mIdTypePairs[resultId2] = typeDescription;

		mIdTypePairs[resultId] = booleanVectorType;
		Push(spv::OpFOrdGreaterThanEqual, booleanVectorTypeId, resultId, argumentId1, m0vfId);

		uint32_t compositeId1 = GetNextId();
		mIdTypePairs[compositeId1] = typeDescription;
		Push(spv::OpCompositeConstruct, dataTypeId, compositeId1, argumentId3, argumentId3, argumentId3, argumentId3);

		Push(spv::OpSelect, dataTypeId, resultId2, resultId, argumentId2, compositeId1);
	}
	else
	{
		Log(warning) << "Process_CMP - Unsupported data type " << argumentType1.PrimaryType << " " << argumentType2.PrimaryType << " " << argumentType3.PrimaryType << std::endl;
	}

	resultId2 = ApplyWriteMask(resultToken, resultId2);

	PrintTokenInformation("CMP", resultToken, argumentToken1, argumentToken2, argumentToken3);
}

void ShaderConverter::Process_DP2ADD()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();
	uint32_t resultId2 = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	Token argumentToken3 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType3 = GetRegisterType(argumentToken3.i);
	uint32_t argumentId3 = GetSwizzledId(argumentToken3, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	if (typeDescription.PrimaryType == spv::OpTypeVoid)
	{
		typeDescription = mIdTypePairs[argumentId2];
	}

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;
	mIdTypePairs[resultId2] = typeDescription;
	switch (dataType)
	{
	case spv::OpTypeFloat:
		//Write out multiply
		Push(spv::OpDot, dataTypeId, resultId, argumentId1, argumentId2);
		//Write out add
		Push(spv::OpFAdd, dataTypeId, resultId2, resultId, argumentId3);
		break;
	default:
		Log(warning) << "Process_DP2ADD - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId2 = ApplyWriteMask(resultToken, resultId2);

	PrintTokenInformation("DP2ADD", resultToken, argumentToken1, argumentToken2, argumentToken3);
}

void ShaderConverter::Process_LRP()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_VECTOR_4);

	Token argumentToken2 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	Token argumentToken3 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType3 = GetRegisterType(argumentToken3.i);
	uint32_t argumentId3 = GetSwizzledId(argumentToken3, GIVE_ME_VECTOR_4);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	mIdTypePairs[resultId] = typeDescription;

	//Not too sure which of these two formula is correct so for now I'll use mix which is the former.

	//x * (1 - a) + y * a
	//x * (y - a) + a

	switch (dataType)
	{
	case spv::OpTypeBool:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450IMix, argumentId1, argumentId2, argumentId3);
		break;
	case spv::OpTypeInt:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450IMix, argumentId1, argumentId2, argumentId3);
		break;
	case spv::OpTypeFloat:
		Push(spv::OpExtInst, dataTypeId, resultId, mGlslExtensionId, GLSLstd450::GLSLstd450FMix, argumentId1, argumentId2, argumentId3);
		break;
	default:
		Log(warning) << "Process_LRP - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("LRP", resultToken, argumentToken1, argumentToken2, argumentToken3);
}

void ShaderConverter::Process_SINCOS()
{
	Token resultToken = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE resultRegisterType = GetRegisterType(resultToken.i);
	uint32_t resultId = GetNextId();

	Token argumentToken1 = GetNextToken();
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType1 = GetRegisterType(argumentToken1.i);
	uint32_t argumentId1 = GetSwizzledId(argumentToken1, GIVE_ME_SCALAR);

	Token argumentToken2 = (mMajorVersion < 3) ? GetNextToken() : argumentToken1;
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType2 = GetRegisterType(argumentToken2.i);
	uint32_t argumentId2 = GetSwizzledId(argumentToken2, GIVE_ME_VECTOR_4);

	Token argumentToken3 = (mMajorVersion < 3) ? GetNextToken() : argumentToken1;
	_D3DSHADER_PARAM_REGISTER_TYPE argumentRegisterType3 = GetRegisterType(argumentToken3.i);
	uint32_t argumentId3 = GetSwizzledId(argumentToken3, GIVE_ME_VECTOR_4);

	TypeDescription vectorType;
	vectorType.PrimaryType = spv::OpTypeVector;
	vectorType.SecondaryType = spv::OpTypeFloat;
	vectorType.ComponentCount = 4;
	uint32_t vectorTypeId = GetSpirVTypeId(vectorType);

	TypeDescription floatType;
	floatType.PrimaryType = spv::OpTypeFloat;
	uint32_t floatTypeId = GetSpirVTypeId(floatType);

	TypeDescription typeDescription = mIdTypePairs[argumentId1];

	spv::Op dataType = typeDescription.PrimaryType;

	//Type could be pointer and matrix so checks are run separately.
	if (typeDescription.PrimaryType == spv::OpTypePointer)
	{
		//Shift the result type so we get a register instead of a pointer as the output type.
		typeDescription.PrimaryType = typeDescription.SecondaryType;
		typeDescription.SecondaryType = typeDescription.TernaryType;
		typeDescription.TernaryType = spv::OpTypeVoid;
	}

	if (typeDescription.PrimaryType == spv::OpTypeMatrix || typeDescription.PrimaryType == spv::OpTypeVector)
	{
		dataType = typeDescription.SecondaryType;
	}

	uint32_t dataTypeId = GetSpirVTypeId(typeDescription);

	uint32_t cosResultId = GetNextId();
	uint32_t sinResultId = GetNextId();

	mIdTypePairs[resultId] = vectorType;
	mIdTypePairs[cosResultId] = typeDescription;
	mIdTypePairs[sinResultId] = typeDescription;

	switch (dataType)
	{
	case spv::OpTypeFloat:
		//Per the documentation Z is undefined so I'll just stick the same values in Z & W.
		if (typeDescription.PrimaryType == spv::OpTypeVector)
		{
			uint32_t tempId = PushCompositeExtract(argumentId1, 0);
			PushCos(floatTypeId, cosResultId, tempId);
			PushSin(floatTypeId, sinResultId, tempId);
		}
		else
		{
			PushCos(dataTypeId, cosResultId, argumentId1);
			PushSin(dataTypeId, sinResultId, argumentId1);
		}

		Push(spv::OpCompositeConstruct, vectorTypeId, resultId, cosResultId, sinResultId, cosResultId, sinResultId);
		break;
	default:
		Log(warning) << "Process_SINCOS - Unsupported data type " << dataType << std::endl;
		break;
	}

	resultId = ApplyWriteMask(resultToken, resultId);

	PrintTokenInformation("SINCOS", resultToken, argumentToken1, argumentToken2, argumentToken3);
}

vk::UniqueShaderModule ShaderConverter::Convert(uint32_t* shader)
{
	//mConvertedShader = {};
	mInstructions.clear();

	uint32_t stringWordSize = 0;
	uint32_t token = 0;
	uint32_t instruction = 0;
	uint32_t extraInfo = 0;

	mBaseToken = mNextToken = mPreviousToken = shader;

	token = GetNextToken().i;
	mPreviousToken--; //Make Previous token one behind the current token.
	mMajorVersion = D3DSHADER_VERSION_MAJOR(token);
	mMinorVersion = D3DSHADER_VERSION_MINOR(token);

	if ((token & 0xFFFF0000) == 0xFFFF0000)
	{
		mIsVertexShader = false;
		Log(info) << "PS " << mMajorVersion << "." << mMinorVersion << std::endl;
	}
	else
	{
		mIsVertexShader = true;
		Log(info) << "VS" << mMajorVersion << "." << mMinorVersion << std::endl;
	}
	//Probably more info in this word but I'll handle that later.


	//Capability
	mCapabilityInstructions.push_back(Pack(2, spv::OpCapability)); //size,Type
	mCapabilityInstructions.push_back(spv::CapabilityShader); //Capability

	//Import
	mGlslExtensionId = GetNextId();
	std::string importStatement = "GLSL.std.450";
	//The spec says 3+variable but there are only 2 before string literal.
	stringWordSize = 2 + (importStatement.length() / 4);
	if (importStatement.length() % 4 == 0)
	{
		stringWordSize++;
	}
	mExtensionInstructions.push_back(Pack(stringWordSize, spv::OpExtInstImport)); //size,Type
	mExtensionInstructions.push_back(mGlslExtensionId); //Result Id
	PutStringInVector(importStatement, mExtensionInstructions);

	//Memory Model
	mMemoryModelInstructions.push_back(Pack(3, spv::OpMemoryModel)); //size,Type
	mMemoryModelInstructions.push_back(spv::AddressingModelLogical); //Addressing Model
	mMemoryModelInstructions.push_back(spv::MemoryModelGLSL450); //Memory Model

	//mSourceInstructions
	mSourceInstructions.push_back(Pack(3, spv::OpSource)); //size,Type
	mSourceInstructions.push_back(spv::SourceLanguageGLSL); //Source Language
	mSourceInstructions.push_back(400); //Version

	std::string sourceExtension1 = "GL_ARB_separate_shader_objects";
	stringWordSize = 2 + (sourceExtension1.length() / 4);
	if (sourceExtension1.length() % 4 == 0)
	{
		stringWordSize++;
	}
	mSourceExtensionInstructions.push_back(Pack(stringWordSize, spv::OpSourceExtension)); //size,Type
	PutStringInVector(sourceExtension1, mSourceExtensionInstructions);

	std::string sourceExtension2 = "GL_ARB_shading_language_420pack";
	stringWordSize = 2 + (sourceExtension2.length() / 4);
	if (sourceExtension2.length() % 4 == 0)
	{
		stringWordSize++;
	}
	mSourceExtensionInstructions.push_back(Pack(stringWordSize, spv::OpSourceExtension)); //size,Type
	PutStringInVector(sourceExtension2, mSourceExtensionInstructions);

	std::string sourceExtension3 = "GL_GOOGLE_cpp_style_line_directive";
	stringWordSize = 2 + (sourceExtension3.length() / 4);
	if (sourceExtension3.length() % 4 == 0)
	{
		stringWordSize++;
	}
	mSourceExtensionInstructions.push_back(Pack(stringWordSize, spv::OpSourceExtension)); //size,Type
	PutStringInVector(sourceExtension3, mSourceExtensionInstructions);

	std::string sourceExtension4 = "GL_GOOGLE_include_directive";
	stringWordSize = 2 + (sourceExtension4.length() / 4);
	if (sourceExtension4.length() % 4 == 0)
	{
		stringWordSize++;
	}
	mSourceExtensionInstructions.push_back(Pack(stringWordSize, spv::OpSourceExtension)); //size,Type
	PutStringInVector(sourceExtension4, mSourceExtensionInstructions);

	GenerateConstantIndices();
	GenerateConstantBlock();
	GenerateRenderStateBlock();
	if (!mIsVertexShader)
	{
		GenerateSamplerBlock();
		GenerateTextureStageBlock();
	}

	//Start of entry point
	mEntryPointTypeId = GetNextId();
	mEntryPointId = GetNextId();

	Push(spv::OpFunction, GetSpirVTypeId(spv::OpTypeVoid), mEntryPointId, spv::FunctionControlMaskNone, mEntryPointTypeId);
	Push(spv::OpLabel, GetNextId());

	Generate255Constants();
	GenerateYFlipConstants();

	//Read DXBC instructions
	while (token != D3DPS_END())
	{
		mTokenOffset = mNextToken - shader;
		token = GetNextToken().i;
		instruction = GetOpcode(token);
		extraInfo = ((token & D3DSP_OPCODESPECIFICCONTROL_MASK) >> D3DSP_OPCODESPECIFICCONTROL_SHIFT);

		switch (instruction)
		{
		case D3DSIO_NOP:
			//Nothing
			break;
		case D3DSIO_PHASE:
			if (!mIsVertexShader)
			{
				if (mMajorVersion == 1 && mMinorVersion == 4)
				{
					mMajorVersion = 2;
					mMinorVersion = 0;
				}
			}
			break;
		case D3DSIO_RET:
			Log(warning) << "Unsupported instruction D3DSIO_RET." << std::endl;
			break;
		case D3DSIO_ENDLOOP:
			Process_ENDLOOP();
			break;
		case D3DSIO_BREAK:
			Process_BREAK();
			break;
		case D3DSIO_TEXDEPTH:
			Log(warning) << "Unsupported instruction D3DSIO_TEXDEPTH." << std::endl;
			SkipTokens(1);
			break;
		case D3DSIO_TEXKILL:
			Process_TEXKILL();
			break;
		case D3DSIO_BEM:
			Log(warning) << "Unsupported instruction D3DSIO_BEM." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXBEM:
			Process_TEXBEM();
			break;
		case D3DSIO_TEXBEML:
			Log(warning) << "Unsupported instruction D3DSIO_TEXBEML." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXDP3:
			Log(warning) << "Unsupported instruction D3DSIO_TEXDP3." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXDP3TEX:
			Log(warning) << "Unsupported instruction D3DSIO_TEXDP3TEX." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXM3x2DEPTH:
			Log(warning) << "Unsupported instruction D3DSIO_TEXM3x2DEPTH." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXM3x2TEX:
			Log(warning) << "Unsupported instruction D3DSIO_TEXM3x2TEX." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXM3x3:
			Log(warning) << "Unsupported instruction D3DSIO_TEXM3x3." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXM3x3PAD:
			Log(warning) << "Unsupported instruction D3DSIO_TEXM3x3PAD." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXM3x3TEX:
			Log(warning) << "Unsupported instruction D3DSIO_TEXM3x3TEX." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXM3x3VSPEC:
			Log(warning) << "Unsupported instruction D3DSIO_TEXM3x3VSPEC." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXREG2AR:
			Log(warning) << "Unsupported instruction D3DSIO_TEXREG2AR." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXREG2GB:
			Log(warning) << "Unsupported instruction D3DSIO_TEXREG2GB." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_TEXREG2RGB:
			Log(warning) << "Unsupported instruction D3DSIO_TEXREG2RGB." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_LABEL:
			Log(warning) << "Unsupported instruction D3DSIO_LABEL." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_CALL:
			Log(warning) << "Unsupported instruction D3DSIO_CALL." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_LOOP:
			Process_LOOP();
			break;
		case D3DSIO_BREAKP:
			Log(warning) << "Unsupported instruction D3DSIO_BREAKP." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_DSX:
			Log(warning) << "Unsupported instruction D3DSIO_DSX." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_DSY:
			Log(warning) << "Unsupported instruction D3DSIO_DSY." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_IFC:
			Process_IFC(extraInfo);
			break;
		case D3DSIO_IF:
			Process_IF();
			break;
		case D3DSIO_ELSE:
			Process_ELSE();
			break;
		case D3DSIO_ENDIF:
			Process_ENDIF();
			break;
		case D3DSIO_REP:
			Process_REP();
			break;
		case D3DSIO_ENDREP:
			Process_ENDREP();
			break;
		case D3DSIO_NRM:
			Process_NRM();
			break;
		case D3DSIO_MOVA:
			Process_MOVA();
			break;
		case D3DSIO_MOV:
			Process_MOV();
			break;
		case D3DSIO_RCP:
			Process_RCP();
			break;
		case D3DSIO_RSQ:
			Process_RSQ();
			break;
		case D3DSIO_EXP:
			Process_EXP();
			break;
		case D3DSIO_EXPP:
			Process_EXPP();
			break;
		case D3DSIO_LOG:
			Process_LOG();
			break;
		case D3DSIO_LOGP:
			Process_LOGP();
			break;
		case D3DSIO_FRC:
			Process_FRC();
			break;
		case D3DSIO_LIT:
			Log(warning) << "Unsupported instruction D3DSIO_LIT." << std::endl;
			SkipTokens(2);
			break;
		case D3DSIO_ABS:
			Process_ABS();
			break;
		case D3DSIO_TEXM3x3SPEC:
			Log(warning) << "Unsupported instruction D3DSIO_TEXM3x3SPEC." << std::endl;
			SkipTokens(3);
			break;
		case D3DSIO_M4x4:
			Process_M4x4();
			break;
		case D3DSIO_M4x3:
			Process_M4x3();
			break;
		case D3DSIO_M3x4:
			Process_M3x4();
			break;
		case D3DSIO_M3x3:
			Process_M3x3();
			break;
		case D3DSIO_M3x2:
			Process_M3x2();
			break;
		case D3DSIO_CALLNZ:
			Log(warning) << "Unsupported instruction D3DSIO_CALLNZ." << std::endl;
			SkipTokens(3);
			break;
		case D3DSIO_SETP:
			Log(warning) << "Unsupported instruction D3DSIO_SETP." << std::endl;
			SkipTokens(3);
			break;
		case D3DSIO_BREAKC:
			Log(warning) << "Unsupported instruction D3DSIO_BREAKC." << std::endl;
			SkipTokens(3);
			break;
		case D3DSIO_ADD:
			Process_ADD();
			break;
		case D3DSIO_SUB:
			Process_SUB();
			break;
		case D3DSIO_MUL:
			Process_MUL();
			break;
		case D3DSIO_DP3:
			Process_DP3();
			break;
		case D3DSIO_DP4:
			Process_DP4();
			break;
		case D3DSIO_MIN:
			Process_MIN();
			break;
		case D3DSIO_MAX:
			Process_MAX();
			break;
		case D3DSIO_DST:
			Process_DST();
			break;
		case D3DSIO_SLT:
			Process_SLT();
			break;
		case D3DSIO_SGE:
			Process_SGE();
			break;
		case D3DSIO_CRS:
			Process_CRS();
			break;
		case D3DSIO_POW:
			Process_POW();
			break;
		case D3DSIO_DP2ADD:
			Process_DP2ADD();
			break;
		case D3DSIO_LRP:
			Process_LRP();
			break;
		case D3DSIO_SGN:
			Log(warning) << "Unsupported instruction D3DSIO_SGN." << std::endl;
			SkipTokens(4);
			break;
		case D3DSIO_CND:
			Log(warning) << "Unsupported instruction D3DSIO_CND." << std::endl;
			SkipTokens(4);
			break;
		case D3DSIO_CMP:
			Process_CMP();
			break;
		case D3DSIO_SINCOS:
			Process_SINCOS();
			break;
		case D3DSIO_MAD:
			Process_MAD();
			break;
		case D3DSIO_TEXLDD:
			Log(warning) << "Unsupported instruction D3DSIO_TEXLDD." << std::endl;
			SkipTokens(5);
			break;
		case D3DSIO_TEXCOORD:
			Process_TEXCOORD();
			break;
		case D3DSIO_TEX:
			Process_TEX();
			break;
		case D3DSIO_TEXLDL:
			Log(warning) << "Unsupported instruction D3DSIO_TEXLDL." << std::endl;
			SkipTokens(3);
			break;
		case D3DSIO_DCL:
			Process_DCL();
			break;
		case D3DSIO_DEFB:
			Process_DEFB();
			break;
		case D3DSIO_DEFI:
			Process_DEFI();
			break;
		case D3DSIO_DEF:
			Process_DEF();
			break;
		case D3DSIO_COMMENT:
			SkipTokens(((token & 0x0fff0000) >> 16));
			break;
		case D3DSIO_END:
			//Nothing
			break;
		default:
			Log(warning) << "Unsupported instruction " << instruction << "." << std::endl;
			break;
		}

	}

	if (mIsVertexShader)
	{
		GenerateYFlip();
	}

	//After inputs & outputs are defined set the function type with the type id defined earlier.
	GetSpirVTypeId(spv::OpTypeFunction, mEntryPointTypeId);

	//End of entry point
	Push(spv::OpReturn);
	Push(spv::OpFunctionEnd);
	uint32_t outputIndex = 0;

	//EntryPoint
	std::string entryPointName = "main";
	std::vector<uint32_t> interfaceIds;
	typedef std::unordered_map<uint32_t, uint32_t> maptype2;
	for (const auto& inputRegister : mInputRegisters)
	{
		interfaceIds.push_back(inputRegister);

#ifdef _EXTRA_SHADER_DEBUG_INFO
		auto& inputRegisterType = mIdTypePairs[inputRegister];
		auto& inputRegisterName = mNameIdPairs[inputRegister];

		Log(info) << "ShaderConverter::Convert " << inputRegisterName << "(" << inputRegister << ") as " << inputRegisterType << "(Input)" << std::endl;
#endif
	}
	for (const auto& outputRegister : mOutputRegisters)
	{
		interfaceIds.push_back(outputRegister);

#ifdef _EXTRA_SHADER_DEBUG_INFO
		auto& outputRegisterType = mIdTypePairs[outputRegister];
		auto& outputRegisterName = mNameIdPairs[outputRegister];

		Log(info) << "ShaderConverter::Convert " << outputRegisterName << "(" << outputRegister << ") as " << outputRegisterType << "(Output)" << std::endl;
#endif
	}

	//The spec says 4+variable but there are only 3 before the string literal.
	stringWordSize = 3 + (entryPointName.length() / 4) + interfaceIds.size();
	if (entryPointName.length() % 4 == 0)
	{
		stringWordSize++;
	}
	mEntryPointInstructions.push_back(Pack(stringWordSize, spv::OpEntryPoint)); //size,Type
	if (mIsVertexShader)
	{
		mEntryPointInstructions.push_back(spv::ExecutionModelVertex); //Execution Model
	}
	else
	{
		mEntryPointInstructions.push_back(spv::ExecutionModelFragment); //Execution Model
	}
	mEntryPointInstructions.push_back(mEntryPointId); //Entry Point (Id)
	PutStringInVector(entryPointName, mEntryPointInstructions); //Name
	mEntryPointInstructions.insert(std::end(mEntryPointInstructions), std::begin(interfaceIds), std::end(interfaceIds)); //Interfaces

	//Write entry point name.
	PushName(mEntryPointId, entryPointName);

	//ExecutionMode
	if (!mIsVertexShader)
	{
		mExecutionModeInstructions.push_back(Pack(3, spv::OpExecutionMode)); //size,Type
		mExecutionModeInstructions.push_back(mEntryPointId); //Entry Point (Id)
		mExecutionModeInstructions.push_back(spv::ExecutionModeOriginUpperLeft); //Execution Mode
	}
	else
	{
		//TODO: figure out what to do for vertex execution mode.
	}


	//Write SPIR-V header
	const GeneratorMagicNumber generatorMagicNumber = { 1,13 };

	mInstructions.push_back(spv::MagicNumber);
	mInstructions.push_back(0x00010000); //spv::Version
	mInstructions.push_back(generatorMagicNumber.Word); //I don't have a generator number ATM.
	mInstructions.push_back(GetNextId()); //Bound
	mInstructions.push_back(0); //Reserved for instruction schema, if needed

	//Dump other opcodes into instruction collection is required order.
	CombineSpirVOpCodes();

	//Pass the word blob to Vulkan to generate a module.
	
	return CreateSpirVModule();
}

#include <settings.h>

#define ABP_INT_VALUE		0
#define ABP_BOOL_VALUE		1
#define ABP_DOUBLE_VALUE	2
#define ABP_STRING_VALUE	3

#define ABP_SETTING_COUNT	2

#define ABS_EXPOSEDYNLDR	0
#define ABS_GETMODHANDLE	1

struct {
	const char *key;
	int type;
}AbpSettingList[ABP_SETTING_COUNT] =
{
	{"ExposeDynLoads",ABP_BOOL_VALUE},
	{"InclGetModHandle",ABP_BOOL_VALUE}
};

#define STNG_LOAD(id,val) AbpGetSetting(AbpSettingList[id].key,(char *)val,AbpSettingList[id].type)
#define STNG_SAVE(id,val) AbpSetSetting(AbpSettingList[id].key,(char *)val,AbpSettingList[id].type)

Settings AbpSettings;

bool AbpGetSetting(const char *key, char *valueBuf,int valueType)
{
	union {
		int vint;
		double vdouble;
	}val;

	char value[32] = { 0 };
	
	if (BridgeSettingGet("apibreak", key, value))
	{
		switch (valueType)
		{
		case ABP_INT_VALUE:
			val.vint = atoi(value);
			memcpy(valueBuf, &val.vint, sizeof(int));
			break;
		case ABP_BOOL_VALUE:
			*valueBuf = !(*value == '0');
			break;
		case ABP_DOUBLE_VALUE:
			val.vdouble = atof(value);
			memcpy(valueBuf, &val.vdouble, sizeof(double));
			break;
		case ABP_STRING_VALUE:
			strcpy(valueBuf, value);
			break;
		}

		return true;
	}

	return false;
}

bool AbpSetSetting(const char *key, char *value, int valueType)
{
	union {
		char *intptr;
		int vint;
		bool vbool;
		double vdouble;
	}val;

	char valueBuf[32] = { 0 };

	val.intptr = value;

	switch (valueType)
	{
	case ABP_INT_VALUE:
		_itoa(val.vdouble, valueBuf, 10);
		break;
	case ABP_BOOL_VALUE:
		*valueBuf = val.vbool ? '1' : '0';
		*(valueBuf + 1) = 0;
		break;
	case ABP_DOUBLE_VALUE:
		sprintf(valueBuf, "%3.2f", val.vdouble);
		break;
	case ABP_STRING_VALUE:
		strcpy(valueBuf, value);
		break;
	}

	return BridgeSettingSet("apibreak", key, valueBuf);
}

bool AbSettingsLoad()
{
	if (!STNG_LOAD(ABS_EXPOSEDYNLDR, &AbpSettings.exposeDynamicApiLoads))
		AbpSettings.exposeDynamicApiLoads = false;

	if (!STNG_LOAD(ABS_GETMODHANDLE, &AbpSettings.includeGetModuleHandle))
		AbpSettings.includeGetModuleHandle = false;

	return true;
}

bool AbSettingsSave()
{
	if (!STNG_SAVE(ABS_EXPOSEDYNLDR, AbpSettings.exposeDynamicApiLoads))
		return false;

	if (!STNG_SAVE(ABS_GETMODHANDLE, AbpSettings.includeGetModuleHandle))
		return false;

	return true;
}

bool AbSettingsReset()
{
	NOTIMPLEMENTED();
	return false;
}

Settings *AbGetSettings()
{
	return &AbpSettings;
}
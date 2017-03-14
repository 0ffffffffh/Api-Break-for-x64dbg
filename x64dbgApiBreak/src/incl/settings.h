#ifndef __SETTINGS_H_
#define __SETTINGS_H_

#include <corelib.h>

typedef struct
{
	bool exposeDynamicApiLoads;
	bool includeGetModuleHandle;
	bool autoLoadData;
	bool scanAggressive;

}Settings;

bool AbSettingsLoad();

bool AbSettingsSave();

bool AbSettingsReset();

Settings *AbGetSettings();

#endif // !__SETTINGS_H_

#ifndef __SETTINGS_H_
#define __SETTINGS_H_

#include <corelib.h>

typedef struct
{
	bool exposeDynamicApiLoads;

}Settings;

bool AbSettingsLoad();

bool AbSettingsSave();

bool AbSettingsReset();

Settings *AbGetSettings();

#endif // !__SETTINGS_H_

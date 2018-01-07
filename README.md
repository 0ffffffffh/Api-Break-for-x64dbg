# Api Break for x64dbg

Api Break is a [x64dbg](http://www.x64dbg.com) plugin which is aimed to set breakpoints Win32/64 API calls easly. 

![ab1](https://raw.githubusercontent.com/0ffffffffh/0ffffffffh.github.io/master/apibreakaction.gif "Breakpoint on callers")

![ab2](https://user-images.githubusercontent.com/437161/34647882-eb65aadc-f39f-11e7-8c14-f1cc3254d0f0.png "Struct and API call mapper")

####**Features**

* API function entry breakpoint (AEB) 
* Breakpoint at API callers (BAC)
* Auto-populating modules and their API functions used by the process.


#### **API Function Entry Breakpoint (AEB)**

It can be useful when the code does obfuscated or indirect api calls or something similar conditions. This mode is automatically (naturally) handles all API calls for the entire module. Also, this mode gives an option to jump automatically to the API caller when the API entry breakpoint hit. It exposes the original caller using single step callstack backtracing.

#### **Breakpoint at API callers (BAC)**
In technically, this mode is much more flexible and customizable. It does scan dynamically for API calls in specified module or address range. For now, it scans only process's code range. But other features are planned for future development.

If you have any idea, let me know what ideas you have about it.

#####**In-Development features**
 - Dynamically loaded API detection which is made by using [GetProcAddress](https://msdn.microsoft.com/en-us/library/windows/desktop/ms683212%28v=vs.85%29.aspx) (*About 45% implemented*)


#####**Planning features**

 - User specified code range scan for **BAC**
 - User specified module scan for **BAC**
 - User option to listing all linked modules and APIs of process regardless of IAT.


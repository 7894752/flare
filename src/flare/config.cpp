/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Preview settings, key bindings and configuration commands.
===========================================================================
*/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "flare/preview.h"

#define MAX_CONFIG_COMMANDS 29
#define MAX_CONFIG_RECURSION 32
#define MAX_CONFIG_TOKEN 256
#define MAX_CONFIG_LINE 2048
#define MAX_OPTION_ARGS 256
#define OPTION_ARG_BUFFER_SIZE 65536

void Q3Map_Error(const char *format, ...);
void Q3Map_Warning(int level, const char *format, ...);
bool GetToken(const char **text, char *buf, int maxChars, bool crossLine);
bool GetLine(const char **text, char *line, int maxChars);
bool SkipLine(const char **text);
int Key_StringToKeynum(const char *name);
void Key_SetBinding(int key, const char *binding);
bool ExecuteOptions(int argc, char **argv);
bool ExecConfigFile(const char *filename);
void ExecuteConfigLine(const char *text);

struct ConfigCommand {
    const char *name;
    bool (*callback)(const char **text);
};
extern const ConfigCommand configCommands[MAX_CONFIG_COMMANDS];
static int configRecursion = 0;

struct FloatSetting {
    const char *name;
    float value, minimum, maximum;
};

struct IntSetting {
    const char *name;
    int value, minimum, maximum;
};

static FloatSetting floatSettings[] = {
    { "movespeed", 256, 2, 4096 },
    { "m_pitch", 0.022f, -1, 1 },
    { "m_yaw", 0.022f, -1, 1 },
    { "sensitivity", 5, 1, 100 },
    { "r_fov", 90, 1, 170 }
};

static IntSetting intSettings[] = {
    { "r_wireframe", 0, 0, 1 },
    { "r_textured", 1, 0, 1 },
    { "r_fullbright", 0, 0, 1 },
    { "r_showlights", 0, 0, 1 },
    { "r_showgrid", 0, 0, 1 },
    { "r_picklight", 0, 0, 1 },
    { "r_showoccluders", 0, 0, 1 },
    { "r_showleaf", 0, 0, 1 },
    { "r_lockleaf", 0, 0, 1 },
    { "r_showbspleaf", 0, 0, 1 },
    { "r_showbreakpoints", 0, 0, 1 },
    { "r_shownormals", 0, 0, 1 },
    { "leakpoint", 0, 0, 2048 }
};

FloatSetting *FindFloatSetting(const char *name);
IntSetting *FindIntSetting(const char *name);

/*
=============
Preview_CvarFloat
=============
*/
float Preview_CvarFloat(const char *name) {
    FloatSetting *var;
    var = FindFloatSetting(name);
    if (var)
        return var->value;
    Q3Map_Error("no float named '%s'\n", name);
    return 0;
}

/*
=============
FindFloatSetting
=============
*/
FloatSetting *FindFloatSetting(const char *name) {
    unsigned int i;
    for (i = 0; i < sizeof(floatSettings) / sizeof(floatSettings[0]); i++) {
        if (!stricmp(name, floatSettings[i].name))
            return &floatSettings[i];
    }
    return 0;
}

/*
=============
Preview_SetCvarFloat
=============
*/
void Preview_SetCvarFloat(const char *name, float value) {
    FloatSetting *var;
    var = FindFloatSetting(name);
    if (!var)
        Q3Map_Error("no float named '%s'\n", name);
    if (value < var->minimum || value > var->maximum)
        Q3Map_Warning(1, "%f not in range [%f, %f]\n", value, var->minimum, var->maximum);
    else
        var->value = value;
}

/*
=============
Preview_CvarInteger
=============
*/
int Preview_CvarInteger(const char *name) {
    IntSetting *var;
    var = FindIntSetting(name);
    if (var)
        return var->value;
    Q3Map_Error("no int named '%s'\n", name);
    return 0;
}

/*
=============
FindIntSetting
=============
*/
IntSetting *FindIntSetting(const char *name) {
    unsigned int i;
    for (i = 0; i < sizeof(intSettings) / sizeof(intSettings[0]); i++) {
        if (!stricmp(name, intSettings[i].name))
            return &intSettings[i];
    }
    return 0;
}

/*
=============
Preview_SetCvarInteger
=============
*/
void Preview_SetCvarInteger(const char *name, int value) {
    IntSetting *var;
    var = FindIntSetting(name);
    if (!var)
        Q3Map_Error("no int named '%s'\n", name);
    if (value < var->minimum || value > var->maximum)
        Q3Map_Warning(1, "%i not in range [%i, %i]\n", value, var->minimum, var->maximum);
    else
        var->value = value;
}

const Vector3d &GetPickedLightOrigin();
const Vector3d &GetPickedLightDirection();
void GL_SetCameraOrigin(const Vector3d *origin);
void GL_SetCameraDirection(const Vector3d *direction);
void GL_ResetGamma();
bool AddSetting(const char **text, int amount);


void GL_ToggleTextureFilter();
void GL_ToggleGamma();
void GL_PickPolygon();
void GL_ClearSelection();
void GL_IncreaseStepLevel();
void GL_DecreaseStepLevel();
void GL_Continue();

/*
=============
Cmd_Bind
=============
*/
bool Cmd_Bind(const char **text) {
    char buf[MAX_CONFIG_LINE];
    int key;
    if (!GetToken(text, buf, sizeof(buf), false)) {
        Q3Map_Warning(1, "missing key name in bind command\n");
        return false;
    }
    key = Key_StringToKeynum(buf);
    if (key < 0) {
        Q3Map_Warning(1, "unknown key '%s'\n", buf);
        return GetToken(text, buf, sizeof(buf), false);
    }
    GetToken(text, buf, sizeof(buf), false);
    Key_SetBinding(key, buf);
    return true;
}

/*
=============
Cmd_Set
=============
*/
bool Cmd_Set(const char **text) {
    char buf[MAX_CONFIG_TOKEN];
    FloatSetting *fvar;
    IntSetting *ivar;
    if (!GetToken(text, buf, sizeof(buf), false)) {
        Q3Map_Warning(1, "missing cvar name\n");
        return false;
    }
    fvar = FindFloatSetting(buf);
    if (fvar) {
        float value;
        if (!GetToken(text, buf, sizeof(buf), false)) {
            Q3Map_Warning(1, "missing cvar value\n");
            return false;
        }
        value = (float)atof(buf);
        if (value < fvar->minimum || value > fvar->maximum)
            Q3Map_Warning(1, "%g not in range [%g, %g]\n", value, fvar->minimum, fvar->maximum);
        else
            fvar->value = value;
        return true;
    }
    ivar = FindIntSetting(buf);
    if (ivar) {
        int value;
        if (!GetToken(text, buf, sizeof(buf), false)) {
            Q3Map_Warning(1, "missing cvar value\n");
            return false;
        }
        value = atoi(buf);
        if (value < ivar->minimum || value > ivar->maximum)
            Q3Map_Warning(1, "%i not in range [%i, %i]\n", value, ivar->minimum, ivar->maximum);
        else
            ivar->value = value;
        return true;
    }
    Q3Map_Warning(1, "no such cvar '%s'\n", buf);
    return GetToken(text, buf, sizeof(buf), false);
}

/*
=============
Cmd_Toggle

Switch to the farther endpoint of the setting's allowed range.
=============
*/
bool Cmd_Toggle(const char **text) {
    char buf[MAX_CONFIG_TOKEN];
    FloatSetting *fvar;
    IntSetting *ivar;
    if (!GetToken(text, buf, sizeof(buf), false)) {
        Q3Map_Warning(1, "missing cvar name\n");
        return false;
    }
    fvar = FindFloatSetting(buf);
    if (fvar) {
        if (fvar->value - fvar->minimum > fvar->maximum - fvar->value)
            fvar->value = fvar->minimum;
        else
            fvar->value = fvar->maximum;
        return true;
    }
    ivar = FindIntSetting(buf);
    if (ivar) {
        if (ivar->value - ivar->minimum > ivar->maximum - ivar->value)
            ivar->value = ivar->minimum;
        else
            ivar->value = ivar->maximum;
        return true;
    }
    Q3Map_Warning(1, "no such cvar '%s'\n", buf);
    return GetToken(text, buf, sizeof(buf), false);
}

/*
=============
Cmd_Inc
=============
*/
bool Cmd_Inc(const char **text) {
    return AddSetting(text, 1);
}

/*
=============
AddSetting

Add an integer step to either kind of setting and clamp the result.
=============
*/
bool AddSetting(const char **text, int amount) {
    char buf[MAX_CONFIG_TOKEN];
    FloatSetting *fvar;
    IntSetting *ivar;
    if (!GetToken(text, buf, sizeof(buf), false)) {
        Q3Map_Warning(1, "missing cvar name\n");
        return false;
    }
    fvar = FindFloatSetting(buf);
    if (fvar) {
        fvar->value += amount;
        if (fvar->value < fvar->minimum)
            fvar->value = fvar->minimum;
        else if (fvar->value > fvar->maximum)
            fvar->value = fvar->maximum;
        return true;
    }
    ivar = FindIntSetting(buf);
    if (ivar) {
        ivar->value += amount;
        if (ivar->value < ivar->minimum)
            ivar->value = ivar->minimum;
        else if (ivar->value > ivar->maximum)
            ivar->value = ivar->maximum;
        return true;
    }
    Q3Map_Warning(1, "no such cvar '%s'\n", buf);
    return GetToken(text, buf, sizeof(buf), false);
}

/*
=============
Cmd_Dec
=============
*/
bool Cmd_Dec(const char **text) {
    return AddSetting(text, -1);
}

/*
=============
Cmd_Add
=============
*/
bool Cmd_Add(const char **text) {
    char buf[MAX_CONFIG_TOKEN];
    int amount;
    if (!GetToken(text, buf, sizeof(buf), false)) {
        Q3Map_Warning(1, "missing add amount\n");
        return false;
    }
    amount = atoi(buf);
    if (!amount) {
        Q3Map_Warning(1, "add ignored - invalid amount '%s'\n", buf);
        return false;
    }
    return AddSetting(text, amount);
}

/*
=============
Cmd_Quit
=============
*/
bool Cmd_Quit(const char **text) {
    GL_ResetGamma();
    exit(0);
}

/*
=============
Cmd_Options
=============
*/
bool Cmd_Options(const char **text) {
    char argBuffer[OPTION_ARG_BUFFER_SIZE];
    char *argv[MAX_OPTION_ARGS];
    int argc = 0;
    unsigned int used = 0;
    for (;;) {
        if (argc == MAX_OPTION_ARGS) {
            Q3Map_Warning(1, "too many arguments in 'options' command\n");
            return false;
        }
        argv[argc] = argBuffer + used;
        if (!GetToken(text, argv[argc], sizeof(argBuffer) - used, false))
            break;
        if (argv[argc][0] == ';') {
            (*text)--;
            break;
        }
        used += strlen(argv[argc]) + 1;
        argc++;
    }
    if (!argc) {
        Q3Map_Warning(2, "no arguments to 'options' command\n");
        return true;
    }
    ExecuteOptions(argc, argv);
    return true;
}

/*
=============
Cmd_Exec
=============
*/
bool Cmd_Exec(const char **text) {
    char filename[MAX_CONFIG_LINE];
    if (!GetToken(text, filename, sizeof(filename), false)) {
        Q3Map_Warning(1, "missing filename in exec command\n");
        return false;
    }
    ExecConfigFile(filename);
    return true;
}

/*
=============
Cmd_MoveLeftDown
=============
*/
bool Cmd_MoveLeftDown(const char **text) {
    movementFlags |= PREVIEW_MOVE_LEFT;
    return true;
}

/*
=============
Cmd_MoveLeftUp
=============
*/
bool Cmd_MoveLeftUp(const char **text) {
    movementFlags &= ~PREVIEW_MOVE_LEFT;
    return true;
}


/*
=============
Cmd_MoveRightDown
=============
*/
bool Cmd_MoveRightDown(const char **text) {
    movementFlags |= PREVIEW_MOVE_RIGHT;
    return true;
}

/*
=============
Cmd_MoveRightUp
=============
*/
bool Cmd_MoveRightUp(const char **text) {
    movementFlags &= ~PREVIEW_MOVE_RIGHT;
    return true;
}

/*
=============
Cmd_MoveUpDown
=============
*/
bool Cmd_MoveUpDown(const char **text) {
    movementFlags |= PREVIEW_MOVE_UP;
    return true;
}

/*
=============
Cmd_MoveUpUp
=============
*/
bool Cmd_MoveUpUp(const char **text) {
    movementFlags &= ~PREVIEW_MOVE_UP;
    return true;
}

/*
=============
Cmd_MoveDownDown
=============
*/
bool Cmd_MoveDownDown(const char **text) {
    movementFlags |= PREVIEW_MOVE_DOWN;
    return true;
}

/*
=============
Cmd_MoveDownUp
=============
*/
bool Cmd_MoveDownUp(const char **text) {
    movementFlags &= ~PREVIEW_MOVE_DOWN;
    return true;
}

/*
=============
Cmd_ForwardDown
=============
*/
bool Cmd_ForwardDown(const char **text) {
    movementFlags |= PREVIEW_MOVE_FORWARD;
    return true;
}

/*
=============
Cmd_ForwardUp
=============
*/
bool Cmd_ForwardUp(const char **text) {
    movementFlags &= ~PREVIEW_MOVE_FORWARD;
    return true;
}

/*
=============
Cmd_BackDown
=============
*/
bool Cmd_BackDown(const char **text) {
    movementFlags |= PREVIEW_MOVE_BACK;
    return true;
}

/*
=============
Cmd_BackUp
=============
*/
bool Cmd_BackUp(const char **text) {
    movementFlags &= ~PREVIEW_MOVE_BACK;
    return true;
}

/*
=============
Cmd_JumpToLight
=============
*/
bool Cmd_JumpToLight(const char **text) {
    GL_SetCameraOrigin(&GetPickedLightOrigin());
    GL_SetCameraDirection(&GetPickedLightDirection());
    return true;
}

/*
=============
Cmd_LmapFilter
=============
*/
bool Cmd_LmapFilter(const char **text) {
    GL_ToggleTextureFilter();
    return true;
}

/*
=============
Cmd_Overbright
=============
*/
bool Cmd_Overbright(const char **text) {
    GL_ToggleGamma();
    return true;
}

/*
=============
Cmd_ToggleBreakpoint
=============
*/
bool Cmd_ToggleBreakpoint(const char **text) {
    GL_PickPolygon();
    return true;
}

/*
=============
Cmd_ResetBreakpoints
=============
*/
bool Cmd_ResetBreakpoints(const char **text) {
    GL_ClearSelection();
    return true;
}

/*
=============
Cmd_StepIn
=============
*/
bool Cmd_StepIn(const char **text) {
    GL_IncreaseStepLevel();
    return true;
}

/*
=============
Cmd_StepOut
=============
*/
bool Cmd_StepOut(const char **text) {
    GL_DecreaseStepLevel();
    return true;
}

/*
=============
Cmd_Step
=============
*/
bool Cmd_Step(const char **text) {
    GL_Continue();
    return true;
}

/*
=============
ExecConfigFile

Execute the file one line at a time.  Nested exec commands share the
recursion limit and leave the caller's file buffer intact.
=============
*/
bool ExecConfigFile(const char *filename) {
    char line[MAX_CONFIG_LINE];
    char *fileBuffer;
    const char *text;
    int count;
    FILE *file;
    if (configRecursion >= MAX_CONFIG_RECURSION) {
        Q3Map_Warning(1, "infinite recursion protection: config file '%s' aborted\n", filename);
        return false;
    }
    file = fopen(filename, "rb");
    if (!file)
        return false;
    fseek(file, 0, SEEK_END);
    count = ftell(file);
    fileBuffer = new char[count + 1];
    fseek(file, 0, SEEK_SET);
    fread(fileBuffer, 1, count, file);
    fclose(file);
    fileBuffer[count] = 0;
    configRecursion++;
    text = fileBuffer;
    while (GetLine(&text, line, sizeof(line))) {
        if (line[0] == '/' && line[1] == '/')
            SkipLine(&text);
        else
            ExecuteConfigLine(line);
    }
    configRecursion--;
    delete[] fileBuffer;
    return true;
}

/*
=============
ExecuteConfigLine

Dispatch semicolon-separated commands.  Each callback advances the
input cursor past its own arguments.
=============
*/
void ExecuteConfigLine(const char *text) {
    char buf[MAX_CONFIG_TOKEN];
    const char *ptr = text;
    unsigned int i;
    while (GetToken(&ptr, buf, sizeof(buf), false)) {
        for (i = 0; i < sizeof(configCommands) / sizeof(configCommands[0]); i++) {
            if (!stricmp(buf, configCommands[i].name)) {
                if (!configCommands[i].callback(&ptr))
                    return;
                break;
            }
        }
        if (i == sizeof(configCommands) / sizeof(configCommands[0])) {
            Q3Map_Warning(1, "unknown command '%s'\n", buf);
            return;
        }
        if (!GetToken(&ptr, buf, sizeof(buf), false))
            break;
        if (buf[0] != ';' || buf[1]) {
            Q3Map_Warning(1, "commands must be seperated by a single semicolon\n");
            break;
        }
    }
}

const ConfigCommand configCommands[MAX_CONFIG_COMMANDS] = {
    { "set", Cmd_Set },
    { "toggle", Cmd_Toggle },
    { "inc", Cmd_Inc },
    { "dec", Cmd_Dec },
    { "add", Cmd_Add },
    { "bind", Cmd_Bind },
    { "quit", Cmd_Quit },
    { "options", Cmd_Options },
    { "exec", Cmd_Exec },
    { "+moveleft", Cmd_MoveLeftDown },
    { "-moveleft", Cmd_MoveLeftUp },
    { "+moveright", Cmd_MoveRightDown },
    { "-moveright", Cmd_MoveRightUp },
    { "+moveup", Cmd_MoveUpDown },
    { "-moveup", Cmd_MoveUpUp },
    { "+movedown", Cmd_MoveDownDown },
    { "-movedown", Cmd_MoveDownUp },
    { "+forward", Cmd_ForwardDown },
    { "-forward", Cmd_ForwardUp },
    { "+back", Cmd_BackDown },
    { "-back", Cmd_BackUp },
    { "jumptolight", Cmd_JumpToLight },
    { "lmapfilter", Cmd_LmapFilter },
    { "overbright", Cmd_Overbright },
    { "togglebreakpoint", Cmd_ToggleBreakpoint },
    { "resetbreakpoints", Cmd_ResetBreakpoints },
    { "stepin", Cmd_StepIn },
    { "stepout", Cmd_StepOut },
    { "step", Cmd_Step }
};

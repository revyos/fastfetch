#include "common/printing.h"
#include "common/jsonconfig.h"
#include "detection/libc/libc.h"
#include "detection/version/version.h"
#include "modules/version/version.h"
#include "util/stringUtils.h"

#define FF_VERSION_NUM_FORMAT_ARGS 9

void ffPrintVersion(FFVersionOptions* options)
{
    FFVersionResult result;
    ffDetectVersion(&result);

    if(options->moduleArgs.outputFormat.length == 0)
    {
        ffPrintLogoAndKey(FF_VERSION_MODULE_NAME, 0, &options->moduleArgs, FF_PRINT_TYPE_DEFAULT);
        printf("%s %s%s%s (%s)\n", result.projectName, result.version, result.versionTweak, result.debugMode ? "-debug" : "", result.architecture);
    }
    else
    {
        FFLibcResult libcResult;
        FF_STRBUF_AUTO_DESTROY buf = ffStrbufCreate();
        if (!ffDetectLibc(&libcResult))
        {
            ffStrbufSetS(&buf, libcResult.name);
            if (libcResult.version)
            {
                ffStrbufAppendC(&buf, ' ');
                ffStrbufAppendS(&buf, libcResult.version);
            }
        }

        ffPrintFormat(FF_VERSION_MODULE_NAME, 0, &options->moduleArgs, FF_VERSION_NUM_FORMAT_ARGS, (FFformatarg[]){
            {FF_FORMAT_ARG_TYPE_STRING, result.projectName},
            {FF_FORMAT_ARG_TYPE_STRING, result.version},
            {FF_FORMAT_ARG_TYPE_STRING, result.versionTweak},
            {FF_FORMAT_ARG_TYPE_STRING, result.debugMode ? "debug" : "release"},
            {FF_FORMAT_ARG_TYPE_STRING, result.architecture},
            {FF_FORMAT_ARG_TYPE_STRING, result.cmakeBuiltType},
            {FF_FORMAT_ARG_TYPE_STRING, result.compileTime},
            {FF_FORMAT_ARG_TYPE_STRING, result.compiler},
            {FF_FORMAT_ARG_TYPE_STRBUF, &buf},
        });
    }
}

bool ffParseVersionCommandOptions(FFVersionOptions* options, const char* key, const char* value)
{
    const char* subKey = ffOptionTestPrefix(key, FF_VERSION_MODULE_NAME);
    if (!subKey) return false;
    if (ffOptionParseModuleArgs(key, subKey, value, &options->moduleArgs))
        return true;

    return false;
}

void ffParseVersionJsonObject(FFVersionOptions* options, yyjson_val* module)
{
    yyjson_val *key_, *val;
    size_t idx, max;
    yyjson_obj_foreach(module, idx, max, key_, val)
    {
        const char* key = yyjson_get_str(key_);
        if(ffStrEqualsIgnCase(key, "type"))
            continue;

        if (ffJsonConfigParseModuleArgs(key, val, &options->moduleArgs))
            continue;

        ffPrintError(FF_VERSION_MODULE_NAME, 0, &options->moduleArgs, "Unknown JSON key %s", key);
    }
}

void ffGenerateVersionJsonConfig(FFVersionOptions* options, yyjson_mut_doc* doc, yyjson_mut_val* module)
{
    __attribute__((__cleanup__(ffDestroyVersionOptions))) FFVersionOptions defaultOptions;
    ffInitVersionOptions(&defaultOptions);

    ffJsonConfigGenerateModuleArgsConfig(doc, module, &defaultOptions.moduleArgs, &options->moduleArgs);
}

void ffGenerateVersionJsonResult(FF_MAYBE_UNUSED FFVersionOptions* options, yyjson_mut_doc* doc, yyjson_mut_val* module)
{
    FFVersionResult result;
    ffDetectVersion(&result);

    yyjson_mut_val* obj = yyjson_mut_obj_add_obj(doc, module, "result");
    yyjson_mut_obj_add_str(doc, obj, "projectName", result.projectName);
    yyjson_mut_obj_add_str(doc, obj, "architecture", result.architecture);
    yyjson_mut_obj_add_str(doc, obj, "version", result.version);
    yyjson_mut_obj_add_str(doc, obj, "versionTweak", result.versionTweak);
    yyjson_mut_obj_add_str(doc, obj, "cmakeBuiltType", result.cmakeBuiltType);
    yyjson_mut_obj_add_str(doc, obj, "compileTime", result.compileTime);
    yyjson_mut_obj_add_str(doc, obj, "compiler", result.compiler);
    yyjson_mut_obj_add_bool(doc, obj, "debugMode", result.debugMode);

    FFLibcResult libcResult;
    if (ffDetectLibc(&libcResult))
    {
        yyjson_mut_obj_add_null(doc, obj, "libc");
    }
    else
    {
        FF_STRBUF_AUTO_DESTROY buf = ffStrbufCreateS(libcResult.name);
        if (libcResult.version)
        {
            ffStrbufAppendC(&buf, ' ');
            ffStrbufAppendS(&buf, libcResult.version);
        }
        yyjson_mut_obj_add_strbuf(doc, obj, "libc", &buf);
    }
}

void ffPrintVersionHelpFormat(void)
{
    ffPrintModuleFormatHelp(FF_VERSION_MODULE_NAME, "{1} {2}{3} ({5})", FF_VERSION_NUM_FORMAT_ARGS, (const char* []) {
        "Project name",
        "Version",
        "Version tweak",
        "Build type (debug or release)",
        "Architecture",
        "CMake build type (Debug, Release, RelWithDebInfo, MinSizeRel)",
        "Date time when compiling",
        "Compiler used",
        "Libc used"
    });
}

void ffInitVersionOptions(FFVersionOptions* options)
{
    ffOptionInitModuleBaseInfo(
        &options->moduleInfo,
        FF_VERSION_MODULE_NAME,
        ffParseVersionCommandOptions,
        ffParseVersionJsonObject,
        ffPrintVersion,
        ffGenerateVersionJsonResult,
        ffPrintVersionHelpFormat,
        ffGenerateVersionJsonConfig
    );
    ffOptionInitModuleArg(&options->moduleArgs);
}

void ffDestroyVersionOptions(FFVersionOptions* options)
{
    ffOptionDestroyModuleArg(&options->moduleArgs);
}

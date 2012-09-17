/*
 * kerberosapp.c
 *
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * Original source developed by yaSSL (http://www.yassl.com)
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "net_brainvitamins_kerberos_KerberosOperations.h"
#include "kerberosapp.h"

/* Global JNI Variables */
JavaVM* cached_jvm;
jobject cached_obj;

/*
 * Generate NULL-terminated argv array from a string.
 * Note: argv begins with command name, and is
 *       NULL terminated (thus the +2)
 */
void generate_argv(char* input, int argc, char** argv)
{
    int i;
    char* tmp;

    LOGI("Entered generate_argv");
    for (i = 0; i < argc + 2; i++)
    {
        if (i == 0)
        { /* add command name */
            argv[i] = (char*) malloc(5 * sizeof(char*));
            strcpy(argv[i], "kinit");
        }
        else if (i == argc + 1) /* add NULL termination */
            argv[i] = NULL;
        else if (i == 1)
        {
            tmp = strtok(input, " ");
            argv[i] = (char*) malloc((strlen(tmp) + 1) * sizeof(char*));
            strcpy(argv[i], tmp);
        }
        else
        {
            tmp = strtok(NULL, " ");
            argv[i] = (char*) malloc((strlen(tmp) + 1) * sizeof(char*));
            strcpy(argv[i], tmp);
        }
    }
    return;
}

/*
 * Free argv array.
 */
void release_argv(int argc, char** argv)
{
    int i;

    for (i = 0; i < argc; i++)
    {
        free(argv[i]);
    }

    free(argv);
}

/*
 * Make sure the required methods exist on the calling Java object.
 * This function should be called early, when we have complete control over the flow of logic.
 */
int validate_caller(JNIEnv* env, jobject object)
{
    jclass class;
    jthrowable exception;

    class = (*env)->GetObjectClass(env, object);

    (*env)->GetMethodID(env, class, LOG_METHOD_NAME, LOG_METHOD_SIGNATURE);
    exception = (*env)->ExceptionOccurred(env);

    if (exception)
        return 1;

    class = (*env)->GetObjectClass(env, object);
    (*env)->GetMethodID(env, class, KINIT_PROMPTER_METHOD_NAME,
            KINIT_PROMPTER_METHOD_SIGNATURE);
    exception = (*env)->ExceptionOccurred(env);

    if (exception)
        return 1;

    return 0;
}

/*
 * Get the current JNIEnv pointer from global JavaVM.
 */
JNIEnv* GetJNIEnv(JavaVM *jvm)
{
    JNIEnv *env;
    int status;

    status = (*jvm)->GetEnv(jvm, (void **) &env, JNI_VERSION_1_6);
    if (status < 0)
    {
        LOGI("Unable to get JNIEnv pointer from JavaVM");
        return NULL;
    }

    return env;
}

/*
 * Is called automatically when library is loaded.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved)
{
    JNIEnv* env;

    LOGI("Loaded Kerberos library");

    if ((*jvm)->GetEnv(jvm, (void**) &env, JNI_VERSION_1_6) != JNI_OK)
        return -1;

    /* Cache our JavaVM pointer */
    cached_jvm = jvm;

    return JNI_VERSION_1_6;
}

/*
 * Is called automatically when library is unloaded.
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *jvm, void *reserved)
{
    JNIEnv *env;

    if( (env = GetJNIEnv(jvm)) == NULL)
    {
        return;
    }

    (*env)->DeleteGlobalRef(env, cached_obj);
    return;

}

/*
 * Class:     net_brainvitamins_kerberos_KerberosOperations
 * Method:    nativeSetKRB5CCNAME
 * Signature: (Ljava/lang/String)I
 *
 * Set an environment variable (e.g. KRB5CCNAME)
 *
 */
JNIEXPORT jint JNICALL Java_net_brainvitamins_kerberos_KerberosOperation_nativeSetEnv(
        JNIEnv* env, jobject obj, jstring variable_name, jstring value)
{
    jboolean isCopy;
    int ret;
    const char *native_variable_name;
    const char *native_value;

    native_variable_name = (*env)->GetStringUTFChars(env, variable_name, NULL);
    native_value = (*env)->GetStringUTFChars(env, value, NULL);

    ret = setenv(native_variable_name, native_value, 1);

    (*env)->ReleaseStringUTFChars(env, variable_name, native_variable_name);
    (*env)->ReleaseStringUTFChars(env, value, native_value);

    return ret;
}

/*
 * Class:     net_brainvitamins_kerberos_KerberosOperations
 * Method:    nativeKinit
 * Signature: (Ljava/lang/String;I)I
 *
 * Wrapper around native kinit application
 *
 */
JNIEXPORT jint JNICALL Java_net_brainvitamins_kerberos_KinitOperation_nativeKinit(
        JNIEnv* env, jobject obj, jstring argString, jint argCount)
{
    jboolean isCopy;
    int ret;
    int numArgs = (int) argCount;
    const char *args;
    char *args_copy;
    char **argv = (char**) malloc((numArgs + 2) * sizeof(char*));

    /* Cache a reference to the calling object */
    cached_obj = (*env)->NewGlobalRef(env, obj);

    if (validate_caller(env, obj))
        return 1;

    /* get original argv string from Java */
    args = (*env)->GetStringUTFChars(env, argString, &isCopy);

    /* make a copy of argString to use with strtok */
    args_copy = malloc(strlen(args) + 1);
    strcpy(args_copy, args);

    /* free argString */
    (*env)->ReleaseStringUTFChars(env, argString, args);

    /* generate argv list */
    generate_argv(args_copy, numArgs, argv);

    /* run kinit */
    ret = kinit_driver(env, obj, numArgs + 1, argv);

    free(args_copy);
    release_argv(numArgs + 1, argv);

    if (ret == 1)
        return 1;
    return 0;
}

/*
 * Class:     net_brainvitamins_kerberos_KerberosOperations
 * Method:    nativeKlist
 * Signature: (Ljava/lang/String;I)I
 *
 * Wrapper around native klist application
 *
 */
JNIEXPORT jint JNICALL Java_net_brainvitamins_kerberos_KerberosOperations_nativeKlist(
        JNIEnv* env, jobject obj, jstring argString, jint argCount)
{
    jboolean isCopy;
    int ret;
    int numArgs = (int) argCount;
    const char *args;
    char *args_copy;
    char **argv = (char**) malloc((numArgs + 2) * sizeof(char*));

    /* Cache a reference to the calling object */
    cached_obj = (*env)->NewGlobalRef(env, obj);

    if (validate_caller(env, obj))
        return 1;

    /* get original argv string from Java */
    args = (*env)->GetStringUTFChars(env, argString, &isCopy);

    /* make a copy of argString to use with strtok */
    args_copy = malloc(strlen(args) + 1);
    strcpy(args_copy, args);

    /* free argString */
    (*env)->ReleaseStringUTFChars(env, argString, args);

    /* generate argv list */
    generate_argv(args_copy, numArgs, argv);

    /* run kinit */
    ret = klist_driver(env, obj, numArgs + 1, argv);

    free(args_copy);
    release_argv(numArgs + 1, argv);

    if (ret == 1)
        return 1;
    return 0;
}

/*
 * Class:     net_brainvitamins_kerberos_KerberosOperations
 * Method:    nativeKvno
 * Signature: (Ljava/lang/String;I)I
 *
 * Wrapper around native kvno application
 *
 */
JNIEXPORT jint JNICALL Java_net_brainvitamins_kerberos_KerberosOperations_nativeKvno(
        JNIEnv* env, jobject obj, jstring argString, jint argCount)
{
    jboolean isCopy;
    int ret;
    int numArgs = (int) argCount;
    const char *args;
    char *args_copy;
    char **argv = (char**) malloc((numArgs + 2) * sizeof(char*));

    /* Cache a reference to the calling object */
    cached_obj = (*env)->NewGlobalRef(env, obj);

    if (validate_caller(env, obj))
        return 1;

    /* get original argv string from Java */
    args = (*env)->GetStringUTFChars(env, argString, &isCopy);

    /* make a copy of argString to use with strtok */
    args_copy = malloc(strlen(args) + 1);
    strcpy(args_copy, args);

    /* free argString */
    (*env)->ReleaseStringUTFChars(env, argString, args);

    /* generate argv list */
    generate_argv(args_copy, numArgs, argv);

    /* run kinit */
    ret = kvno_driver(env, obj, numArgs + 1, argv);

    free(args_copy);
    release_argv(numArgs + 1, argv);

    if (ret == 1)
        return 1;
    return 0;
}

/*
 * Class:     net_brainvitamins_kerberos_KerberosOperations
 * Method:    nativeKdestroy
 * Signature: (Ljava/lang/String;I)I
 *
 * Wrapper around native kdestroy application
 *
 */
JNIEXPORT jint JNICALL Java_net_brainvitamins_kerberos_KerberosOperations_nativeKdestroy(
        JNIEnv* env, jobject obj, jstring argString, jint argCount)
{
    jboolean isCopy;
    int ret;
    int numArgs = (int) argCount;
    const char *args;
    char *args_copy;
    char **argv = (char**) malloc((numArgs + 2) * sizeof(char*));

    /* Cache a reference to the calling object */
    cached_obj = (*env)->NewGlobalRef(env, obj);

    if (validate_caller(env, obj))
        return 1;

    /* get original argv string from Java */
    args = (*env)->GetStringUTFChars(env, argString, &isCopy);

    /* make a copy of argString to use with strtok */
    args_copy = malloc(strlen(args) + 1);
    strcpy(args_copy, args);

    /* free argString */
    (*env)->ReleaseStringUTFChars(env, argString, args);

    /* generate argv list */
    generate_argv(args_copy, numArgs, argv);

    /* run kdestroy */
    ret = kdestroy_driver(env, obj, numArgs + 1, argv);

    free(args_copy);
    release_argv(numArgs + 1, argv);

    if (ret == 1)
        return 1;
    return 0;
}

/*
 * Android log function, printf-style.
 */
void android_log(const char* format, ...)
{
    va_list args;
    char appendString[4096];

    va_start(args, format);
    vsnprintf(appendString, sizeof(appendString), format, args);
    android_log_message(appendString);
    va_end(args);
}

/*
 * Android error log function, replaces com_err calls
 */
void android_log_error(const char* progname, errcode_t code, const char* format,
        ...)
{
    va_list args;
    char errString[4096] = "Error ";
    char errCodeString[16];

    //insert error code
    snprintf(errCodeString, sizeof(errCodeString), "%ld", code);
    strcat(errString, errCodeString);
    strcat(errString, " ");

    va_start(args, format);
    vsnprintf(errString + strlen(errString), sizeof(errString), format, args);
    strcat(errString, "\n");

    android_log_message(errString);
    va_end(args);
}

/*
 * Log a message to the Java environment
 *
 * Note: Set jni_env, class_obj before calling.
 * Return: 0 (success), 1 (failure)
 */
int android_log_message(char* input)
{
    JNIEnv* env;
    jclass cls;
    jmethodID mid;
    jstring javaOutput;

    env = GetJNIEnv(cached_jvm);
    cls = (*env)->GetObjectClass(env, cached_obj);
    mid = (*env)->GetMethodID(env, cls, LOG_METHOD_NAME, LOG_METHOD_SIGNATURE);

    // doing exception checking here does no good since the VM will terminate
    // if other functions are called after the exception occurs.
    // We can't control the behavior of functions that call this function,
    // so we need to do the error checking upstream (in validate_error)
    javaOutput = (*env)->NewStringUTF(env, input);
    if (env == NULL || cached_obj == NULL || mid == NULL || javaOutput == NULL)
    {
        LOGI("Null variable in android_log_message input.");
        return 1;
    }
    (*env)->CallVoidMethod(env, cached_obj, mid, javaOutput);

    return 0;
}

/* @(#) $Id$ */

/* Copyright (C) 2005-2007 Daniel B. Cid <dcid@ossec.net>
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

       
/* Windows only */
#ifdef WIN32

       
#include "shared.h"
#include "syscheck.h"
#include "os_crypto/md5/md5_op.h"
#include "os_crypto/sha1/sha1_op.h"


/* Default values */
#define MAX_KEY_LENGTH 255
#define MAX_KEY	2048
#define MAX_VALUE_NAME 16383
 
 
/* Global variable */
HKEY sub_tree;

/** Function prototypes 8*/
void os_winreg_open_key(char *subkey);


int os_winreg_changed(char *key, char *md5, char *sha1)
{
    char buf[MAX_LINE +1];
    
    buf[MAX_LINE] = '\0';


    /* Seeking to the beginning of the db */
    fseek(syscheck.reg_fp, 0, SEEK_SET);

    while(fgets(buf, MAX_LINE, syscheck.reg_fp) != NULL)
    {
        if((buf[0] != '#') && (buf[0] != ' ') && (buf[0] != '\n'))
        {
            char *n_buf;

            /* Removing the \n before reading */
            n_buf = strchr(buf, '\n');
            if(n_buf == NULL)
                continue;

            *n_buf = '\0';    
            
            n_buf = strchr(buf, ' ');
            if(n_buf == NULL)
                continue;
            
            if(strcmp(n_buf +1, key) != 0)
                continue;
            
            /* Entry found, checking if checksum is the same */
            *n_buf = '\0';    
            if((strncmp(buf, md5, sizeof(os_md5) -1) == 0)&&
               (strcmp(buf + sizeof(os_md5) -1, sha1) == 0))
            {
                /* File didn't change. */
                return(0);
            }

            /* File did changed */
            return(1);
        }
    }

    fseek(syscheck.reg_fp, 0, SEEK_END);
    fprintf(syscheck.reg_fp, "%s%s %s\n", md5, sha1, key);
    return(1);
}


/** int notify_registry(char *msg)
 * Notifies of registry changes.
 */
int notify_registry(char *msg)
{
    if(SendMSG(syscheck.queue, msg, SYSCHECK_REG, SYSCHECK_MQ) < 0)
    {
        merror(QUEUE_SEND, ARGV0);

        if((syscheck.queue = StartMQ(DEFAULTQPATH,WRITE)) < 0)
        {
            ErrorExit(QUEUE_FATAL, ARGV0, DEFAULTQPATH);
        }

        /* If we reach here, we can try to send it again */
        SendMSG(syscheck.queue, msg, SYSCHECK, SYSCHECK_MQ);
    }
    return(0);
}


/** char *os_winreg_sethkey(char *reg_entry)
 * Checks if the registry entry is valid.
 */
char *os_winreg_sethkey(char *reg_entry)
{
    char *ret = NULL;
    char *tmp_str;

    /* Getting only the sub tree first */
    tmp_str = strchr(reg_entry, '\\');
    if(tmp_str)
    {
        *tmp_str = '\0';
        ret = tmp_str+1;
    }

    /* Setting sub tree */
    if(strcmp(reg_entry, "HKEY_LOCAL_MACHINE") == 0)
    {
        sub_tree = HKEY_LOCAL_MACHINE;
    }
    else if(strcmp(reg_entry, "HKEY_CLASSES_ROOT") == 0)
    {
        sub_tree = HKEY_CLASSES_ROOT;
    }
    else if(strcmp(reg_entry, "HKEY_CURRENT_CONFIG") == 0)
    {
        sub_tree = HKEY_CURRENT_CONFIG;
    }
    else if(strcmp(reg_entry, "HKEY_USERS") == 0)
    {
        sub_tree = HKEY_USERS;
    }
    else
    {
        /* Returning tmp_str to the previous value */
        if(tmp_str && (*tmp_str == '\0'))
            *tmp_str = '\\';
        return(NULL);
    }

    /* Checking if ret has nothing else. */
    if(ret && (*ret == '\0'))
        ret = NULL;
        
    return(ret);
}


/* void os_winreg_querykey(HKEY hKey, char *p_key)
 * Query the key and get all its values.
 */
void os_winreg_querykey(HKEY hKey, char *p_key) 
{
    int i, rc;
    DWORD j;

    /* QueryInfo and EnumKey variables */
    TCHAR sub_key_name_b[MAX_KEY_LENGTH +1];
    TCHAR class_name_b[MAX_PATH +1];
    DWORD sub_key_name_s;
    DWORD class_name_s = MAX_PATH;

    /* Number of sub keys */
    DWORD subkey_count = 0;

    /* Number of values */
    DWORD value_count;

    /* Variables for RegEnumValue */
    TCHAR value_buffer[MAX_VALUE_NAME +1]; 
    TCHAR data_buffer[MAX_VALUE_NAME +1]; 
    DWORD value_size;
    DWORD data_size;

    /* Data type for RegEnumValue */
    DWORD data_type = 0;


    /* Initializing the memory for some variables */
    class_name_b[0] = '\0';
    class_name_b[MAX_PATH] = '\0';
    sub_key_name_b[0] = '\0';
    sub_key_name_b[MAX_KEY_LENGTH] = '\0';
    

    /* We use the class_name, subkey_count and the value count. */
    rc = RegQueryInfoKey(hKey, class_name_b, &class_name_s, NULL,
            &subkey_count, NULL, NULL, &value_count,
            NULL, NULL, NULL, NULL);

    /* Check return code of QueryInfo */
    if(rc != ERROR_SUCCESS)
    {
        return;
    }



    /* Checking if we have sub keys */
    if(subkey_count)
    {
        /* We open each subkey and call open_key */
        for(i=0;i<subkey_count;i++) 
        { 
            sub_key_name_s = MAX_KEY_LENGTH;
            rc = RegEnumKeyEx(hKey, i, sub_key_name_b, &sub_key_name_s,
                              NULL, NULL, NULL, NULL); 
            
            /* Checking for the rc. */
            if(rc == ERROR_SUCCESS) 
            {
                char new_key[MAX_KEY_LENGTH + 2];
                new_key[MAX_KEY_LENGTH +1] = '\0';

                if(p_key)
                {
                    snprintf(new_key, MAX_KEY_LENGTH, 
                                      "%s\\%s", p_key, sub_key_name_b);
                }
                else
                {
                    snprintf(new_key, MAX_KEY_LENGTH, "%s", sub_key_name_b);
                }

                /* Opening subkey */
                os_winreg_open_key(new_key);
            }
        }
    }
    
    /* Getting Values (if available) */
    if (value_count) 
    {
        /* md5 and sha1 sum */
        os_md5 mf_sum;
        os_sha1 sf_sum;

        FILE *checksum_fp;

        char *mt_data;


        /* Clearing the values for value_size and data_size */
        value_buffer[MAX_VALUE_NAME] = '\0';
        data_buffer[MAX_VALUE_NAME] = '\0';
        checksum_fp = fopen(SYS_REG_TMP, "w");
        if(!checksum_fp)
        {
            printf(FOPEN_ERROR, ARGV0, SYS_REG_TMP);
            return;
        }

        /* Getting each value */
        for(i=0;i<value_count;i++) 
        { 
            value_size = MAX_VALUE_NAME; 
            data_size = MAX_VALUE_NAME;

            value_buffer[0] = '\0';
            data_buffer[0] = '\0';

            rc = RegEnumValue(hKey, i, value_buffer, &value_size,
                    NULL, &data_type, data_buffer, &data_size);

            /* No more values available */
            if(rc != ERROR_SUCCESS)
            {
                break;
            }

            /* Checking if no value name is specified */
            if(value_buffer[0] == '\0')
            {
                value_buffer[0] = '@';
                value_buffer[1] = '\0';
            }

            /* Writing valud name and data in the file (for checksum later) */
            fprintf(checksum_fp, "%s=", value_buffer);
            switch(data_type)
            {
                case REG_SZ:
                case REG_EXPAND_SZ:
                    fprintf(checksum_fp, "%s\n", data_buffer);
                    break;
                case REG_MULTI_SZ:
                    /* Printing multiple strings */
                    mt_data = data_buffer;

                    while(*mt_data)
                    {
                        fprintf(checksum_fp, "%s ", mt_data);
                        mt_data += strlen(mt_data) +1;
                    }
                    fprintf(checksum_fp, "\n");
                    break;
                case REG_DWORD:
                    fprintf(checksum_fp, "%08x\n",(unsigned int)*data_buffer);
                    break;
                default:
                    for(j = 0;j<data_size;j++)
                    {
                        fprintf(checksum_fp, "%02x",
                                (unsigned int)data_buffer[j]);
                    }
                    fprintf(checksum_fp, "\n");
                    break;	
            }
        }

        /* Generating checksum of the values */
        fclose(checksum_fp);

        if(OS_MD5_File(SYS_REG_TMP, mf_sum) == -1)
        {
            merror(FOPEN_ERROR, ARGV0, SYS_REG_TMP);
            return;
        }
        if(OS_SHA1_File(SYS_REG_TMP, sf_sum) == -1)
        {
            merror(FOPEN_ERROR, ARGV0, SYS_REG_TMP);
            return;
        }

        /* Looking for p_key on the reg db */
        if(os_winreg_changed(p_key, mf_sum, sf_sum))
        {
            char reg_changed[MAX_LINE +1];
            snprintf(reg_changed, MAX_LINE, "0:0:0:0:%s:%s %s",
                                  mf_sum, sf_sum, p_key); 

            /* Notifying server */
            notify_registry(reg_changed);
        }
    }
}


/* int os_winreg_open_key(char *subkey)
 * Open the registry key
 */
void os_winreg_open_key(char *subkey)
{
    int i = 0;	
    HKEY oshkey;


    /* Registry ignore list */
    if(subkey)
    {
        while(syscheck.registry_ignore[i] != NULL)
        {
            if(strcasecmp(syscheck.registry_ignore[i], subkey) == 0)
            {
                return;
            }
            i++;
        }
    }

    if(RegOpenKeyEx(sub_tree, subkey, 0, KEY_READ, &oshkey) != ERROR_SUCCESS)
    {
        merror(SK_REG_OPEN, ARGV0, subkey);
        return;
    }

    os_winreg_querykey(oshkey, subkey);
    RegCloseKey(sub_tree);
    return;
}


/** void os_winreg_check()
 * Main function to read the registry.
 */
void os_winreg_check()
{
    int i = 0;
    char *rk;


    /* Checking if the registry fp is open */
    if(syscheck.reg_fp == NULL)
    {
        syscheck.reg_fp = fopen(SYS_WIN_REG, "w+");
        if(!syscheck.reg_fp)
        {
            merror(FOPEN_ERROR, ARGV0, SYS_WIN_REG);
            return;
        }
    }
    

    /* Getting sub class and a valid registry entry */
    while(syscheck.registry[i] != NULL)
    {
        sub_tree = NULL;
        rk = NULL;
        
        /* Ignored entries are zeroed */
        if(*syscheck.registry[i] == '\0')
            continue;
            
        rk = os_winreg_sethkey(syscheck.registry[i]);
        if(sub_tree == NULL)
        {
            merror(SK_INV_REG, ARGV0, syscheck.registry[i]);
            *syscheck.registry[i] = '\0';
            i++;
            continue;
        }

        os_winreg_open_key(rk);
        i++;
    }
    return;
}


#endif /* WIN32 */

/* EOF */

/*  Copyright 2007-2017 the original vcontrold development team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Routines for reading XML files

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>
#include <arpa/inet.h>

#include "common.h"
#include "parser.h"

#include "xmlconfig.h"


// Declarations
static protocolPtr newProtocolNode(protocolPtr ptr);
static char *getPropertyNode(xmlAttrPtr cur, xmlChar *name);
static enumPtr newEnumNode(enumPtr ptr);
static void removeProtocolList(protocolPtr ptr);
static void removeUnitList(unitPtr ptr);
static void removeMacroList(macroPtr ptr);
static void removeCommandList(commandPtr ptr);
static void removeDeviceList(devicePtr ptr);
static void removeIcmdList(icmdPtr ptr);
static void removeAllowList(allowPtr ptr);
static void removeEnumList(enumPtr ptr);
static void freeAllLists();
static xmlNodePtr getNextNode(xmlNodePtr cur, xmlNodePtr prevPtr);


// Global variables
static protocolPtr protoPtr = NULL;
static unitPtr uPtr = NULL;
static devicePtr devPtr = NULL;
static configPtr cfgPtr = NULL;
static commandPtr cmdPtr = NULL;

configPtr getConfig()
{
    if (! cfgPtr)
    {
        logIT(LOG_ERR, "no xml configuration loaded");
    }
    return cfgPtr;
}

static protocolPtr newProtocolNode(protocolPtr ptr)
{
    protocolPtr nptr;

    if (ptr && ptr->next) {
        // recurse to the end of the list
        return newProtocolNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Protocol));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }
    nptr->next = NULL;
    nptr->icPtr = NULL;
    nptr->id = 0;

    return nptr;
}

static protocolPtr getProtocolNode(protocolPtr ptr, const char *name)
{
    if (ptr && strcmp(ptr->name, name) != 0) {
        // recurse to the next node
        return getProtocolNode(ptr->next, name);
    }

    return ptr;
}

static void removeProtocolList(protocolPtr ptr)
{
    if (ptr && ptr->next) {
        // recurse to the end of the list
        removeProtocolList(ptr->next);
    }

    if (ptr) {
        removeMacroList(ptr->mPtr);
        removeIcmdList(ptr->icPtr);
        free(ptr->name);
        free(ptr);
    }
}

static unitPtr newUnitNode(unitPtr ptr)
{
    unitPtr nptr;

    if (ptr && ptr->next) {
        // recurse to the end of the list
        return newUnitNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Unit));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    return nptr;
}

unitPtr getUnitNode(unitPtr ptr, const char *name)
{
    if (ptr && ptr->abbrev && (strcmp(ptr->abbrev, name) != 0)) {
        return getUnitNode(ptr->next, name);
    }

    return ptr;
}

static void removeUnitList(unitPtr ptr)
{
    if (ptr && ptr->next) {
        // recurse to the end of the list
        removeUnitList(ptr->next);
    }

    if (ptr) {
        removeEnumList(ptr->ePtr);
        free(ptr->name);
        free(ptr->abbrev);
        free(ptr->gCalc);
        free(ptr->sCalc);
        free(ptr->gICalc);
        free(ptr->sICalc);
        free(ptr->entity);
        free(ptr->type);
        free(ptr);
    }
}

static macroPtr newMacroNode(macroPtr ptr)
{
    macroPtr nptr;

    if (ptr && ptr->next) {
        // recurse to the end of the list
        return newMacroNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Macro));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    return nptr;
}

macroPtr getMacroNode(macroPtr ptr, const char *name)
{
    if (ptr && ptr->name && strcmp(ptr->name, name) != 0) {
        // recurse to the end of the list
        return getMacroNode(ptr->next, name);
    }

    return ptr;
}

static void removeMacroList(macroPtr ptr)
{
    if (ptr && ptr->next) {
        removeMacroList(ptr->next);
    }

    if (ptr) {
        free(ptr->name);
        free(ptr->command);
        free(ptr);
    }
}

commandPtr newCommandNode(commandPtr ptr)
{
    commandPtr nptr;

    if (ptr && ptr->next) {
        // recurse to the end of the list
        return newCommandNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Command));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    nptr->cmpPtr = NULL;
    nptr->bit = -1;

    return nptr;
}

void addCommandNode(commandPtr ptr, commandPtr nptr)
{
    if (ptr) {
        if (ptr->next) {
            addCommandNode(ptr->next, nptr);
        } else {
            ptr->next = nptr;
        }
    }
}

commandPtr getCommandNode(commandPtr ptr, const char *name)
{
    if (ptr && ptr->name && name && (strcmp(ptr->name, name) != 0)) {
        return getCommandNode(ptr->next, name);
    }

    return ptr;
}

static void removeCommandList(commandPtr ptr)
{
    if (ptr && ptr->next) {
        // recurse to the end of the list
        removeCommandList(ptr->next);
    }

    if (ptr) {
        removeCompileList(ptr->cmpPtr);
        free(ptr->send);
        if (ptr->nodeType > 0) {
            // Has been read from the XML file
            if (ptr->addr) {
                free(ptr->addr);
            }
            if (ptr->precmd) {
                free(ptr->precmd);
            }
            if (ptr->unit) {
                free(ptr->unit);
            }
            if (ptr->pcmd) {
                free(ptr->pcmd);
            }
            if (ptr->errStr) {
                free(ptr->errStr);
            }
            if (ptr->nodeType == 1) {
                // Originalknoten
                if (ptr->name) {
                    free(ptr->name);
                }
                if (ptr->description) {
                    free(ptr->description);
                }
            }
        }
        free(ptr);
    }
}

devicePtr newDeviceNode(devicePtr ptr)
{
    devicePtr nptr;

    if (ptr && ptr->next) {
        // recurse to the end of the list
        return newDeviceNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Device));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    return nptr;
}

static devicePtr getDeviceNode(devicePtr ptr, char *id)
{
    if (ptr && strcmp(ptr->id, id) != 0) {
        return getDeviceNode(ptr->next, id);
    }

    return ptr;
}

static void removeDeviceList(devicePtr ptr)
{
    if (ptr && ptr->next) {
        // recurse to the end of the list
        removeDeviceList(ptr->next);
    }
    if (ptr) {
        removeCommandList(ptr->cmdPtr);
        free(ptr->name);
        free(ptr->id);
        free(ptr);
    }
}

static icmdPtr newIcmdNode(icmdPtr ptr)
{
    icmdPtr nptr;

    if (ptr && ptr->next) {
        // recurse to the end of the list
        return newIcmdNode(ptr->next);
    }

    nptr = calloc(1, sizeof(iCmd));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }

    nptr->next = NULL;
    return nptr;
}

icmdPtr getIcmdNode(icmdPtr ptr, const char *name)
{
    if (ptr && ptr->name && (strcmp(ptr->name, name) != 0)) {
        return getIcmdNode(ptr->next, name);
    }

    return ptr;
}

static void removeIcmdList(icmdPtr ptr)
{
    if (ptr && ptr->next) {
        // recurse to the end of the list
        removeIcmdList(ptr->next);
    }
    if (ptr) {
        free(ptr->name);
        free(ptr->send);
        free(ptr);
    }
}

allowPtr getAllowNode(allowPtr ptr, const in_addr_t testIP)
{
    if (! ptr) {
        return NULL;
    }

    if ((ntohl(ptr->ip) & ptr->mask) == (ntohl(testIP) & ptr->mask)) {
        return ptr;
    }

    // recurse into next node
    return getAllowNode(ptr->next, testIP);
}

static allowPtr newAllowNode(allowPtr ptr)
{
    if (ptr && ptr->next) {
        // recurse to the end of the list
        return newAllowNode(ptr->next);
    }

    allowPtr nptr = calloc(1, sizeof(Allow));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }

    return nptr;
}

static void removeAllowList(allowPtr ptr)
{
    if (ptr && ptr->next) {
        // recurse to the end of the list
        removeAllowList(ptr->next);
    }

    if (ptr) {
        free(ptr->text);
        free(ptr);
    }
}

static enumPtr newEnumNode(enumPtr ptr)
{
    enumPtr nptr;

    if (ptr && ptr->next) {
        // recurse to the end of the list
        return newEnumNode(ptr->next);
    }

    nptr = calloc(1, sizeof(Enumerate));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }

    return nptr;
}

enumPtr getEnumNode(enumPtr ptr, char *search, int len)
{
    if (! ptr) {
        return NULL;
    }

    if ((len > 0) && ptr->bytes && (memcmp(ptr->bytes, search, len) == 0)) {
        // len given, we search for bytes
        return ptr;
    } else if (!len && ptr->text && (strcmp(ptr->text, search) == 0)) {
        // len == 0 --> string comparison
        return ptr;
    } else if ((len < 0) && (ptr->bytes == NULL)) {
        // len == -1 --> default value
        return ptr;
    } else {
        return getEnumNode(ptr->next, search, len);
    }
}

static void removeEnumList(enumPtr ptr)
{
    if (ptr && ptr->next) {
        // recurse to the end of the list
        removeEnumList(ptr->next);
    }

    if (ptr) {
        free(ptr->text);
        free(ptr->bytes);
        free(ptr);
    }
}

static void printNode(xmlNodePtr ptr)
{
    static int blanks = 0;
    int n;

    if (! ptr) {
        return;
    }

    for (n = 0; n <= blanks; n++) {
        printf(" ");
    }

    if ((ptr->type == XML_ELEMENT_NODE) || (ptr->type == XML_TEXT_NODE)) {
        printf("(%d) Node::Name=%s Type:%d Content=%s\n",
               ptr->line, ptr->name, ptr->type, ptr->content);
    } else {
        printf("Node::Name=%s\n", ptr->name);
    }

    if ((ptr->type == XML_ELEMENT_NODE) && ptr->properties) {
        blanks++;
        printNode((xmlNodePtr) ptr->properties);
        blanks--;
    }

    if (ptr->children) {
        blanks++;
        printNode(ptr->children);
        blanks--;
    }

    if ( ptr->next) {
        printNode(ptr->next);
    }
}

static char *getTextNode(xmlNodePtr cur)
{
    if ((cur->children) && (cur->children->type == XML_TEXT_NODE)) {
        return (char *)cur->children->content;
    } else {
        return NULL;
    }
}

static char *getPropertyNode(xmlAttrPtr cur, xmlChar *name)
{
    if ((cur) && (cur->type == XML_ATTRIBUTE_NODE) && strstr((char *)cur->name, (char *)name)) {
        return (char *)getTextNode((xmlNodePtr) cur);
    } else if (cur && cur->next) {
        return (char *)getPropertyNode(cur->next, name);
    } else {
        return NULL;
    }
}

void nullIT(char **ptr)
{
    *ptr = calloc(1, sizeof(char));
    **ptr = '\0';
}

static configPtr parseConfig(xmlNodePtr cur)
{
    int serialFound = 0;
    int netFound = 0;
    int logFound = 0;
    configPtr cfgPtr;
    char *chrPtr;
    xmlNodePtr prevPtr;
    allowPtr aPtr;
    char ip[16];

    cfgPtr = calloc(1, sizeof(Config));
    cfgPtr->port = 0;
    cfgPtr->syslog = 0;
    cfgPtr->debug = 0;
    cfgPtr->aPtr = NULL;

    while (cur) {
        logIT(LOG_INFO, "CONFIG:(%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);
        if (strstr((char *)cur->name, "serial"))  {
            serialFound = 1;
            prevPtr = cur;
            cur = cur->children;
        } else if (strstr((char *)cur->name, "net")) {
            netFound = 1;
            prevPtr = cur;
            cur = cur->children;
        } else if (strstr((char *)cur->name, "logging"))  {
            logFound = 1;
            prevPtr = cur;
            cur = cur->children;
        } else if (strstr((char *)cur->name, "device"))  {
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"ID");
            logIT(LOG_INFO, "     Device ID=%s", cfgPtr->devID);
            if (chrPtr) {
                cfgPtr->devID = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(cfgPtr->devID, chrPtr);
            } else {
                nullIT(&cfgPtr->devID);
            }
            cur = cur->next;
        } else if (serialFound && strstr((char *)cur->name, "tty")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                cfgPtr->tty = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(cfgPtr->tty, chrPtr);
            } else {
                nullIT(&cfgPtr->devID);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (netFound && strstr((char *)cur->name, "port"))  {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                cfgPtr->port = atoi(chrPtr);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (netFound && strstr((char *)cur->name, "allow"))  {
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"ip");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s", cur->line, cur->name, cur->type, chrPtr);

            // We now disassemble chrPtr to ip/size.
            // If no mask is given, we assume /32.
            // Afterwards, we build an inverse mask and put it in mask.
            // ip == text content ip address mask == bitmask

            char *ptr;
            short count;
            short size;
            in_addr_t mask;

            bzero(ip, sizeof(ip));
            if ((ptr = strchr(chrPtr, '/'))) {
                size = atoi(ptr + 1);
                strncpy(ip, chrPtr, ptr - chrPtr);
            } else {
                strncpy(ip, chrPtr, sizeof(ip) - 1);
                size = 32;
            }

            if (inet_addr(ip) != INADDR_NONE) {
                aPtr = newAllowNode(cfgPtr->aPtr);
                aPtr->text = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(aPtr->text, chrPtr);
                mask = 0;
                // We assemble a bitmask
                if (size) {
                    mask = 0x80000000;
                    for (count = 0; count < size - 1; count++) {
                        mask >>= 1;
                        mask |= 0x80000000;
                    }
                }
                aPtr->mask = mask;
                aPtr->ip = inet_addr(ip);

                if (! cfgPtr->aPtr) {
                    cfgPtr->aPtr = aPtr;
                }

                logIT(LOG_INFO, "     Allow IP:%s Size:/%d", ip, size);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (logFound && strstr((char *)cur->name, "file")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                cfgPtr->logfile = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(cfgPtr->logfile, chrPtr);
            } else {
                nullIT(&cfgPtr->devID);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (logFound && strstr((char *)cur->name, "syslog")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            cfgPtr->syslog = ((*chrPtr == 'y') || (*chrPtr == '1')) ? 1 : 0;
            cur = getNextNode(cur, prevPtr);
        } else if (logFound && strstr((char *)cur->name, "debug")) {
            chrPtr = getTextNode(cur);
            cfgPtr->debug = ((*chrPtr == 'y') || (*chrPtr == '1')) ? 1 : 0;
            cur = getNextNode(cur, prevPtr);
        } else {
            cur = getNextNode(cur, prevPtr);
        }
    }

    return cfgPtr;
}

static unitPtr parseUnit(xmlNodePtr cur)
{
    unitPtr uPtr;
    unitPtr uStartPtr = NULL;
    char *unit;
    char *chrPtr;
    int unitFound = 0;
    xmlNodePtr prevPtr;
    char string[256];
    enumPtr ePtr;

    while (cur) {
        logIT(LOG_INFO, "UNIT: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }

        if (strstr((char *)cur->name, "unit")) {
            unit = getPropertyNode(cur->properties, (xmlChar *)"name");
            if (unit) {
                // neue Unit gelesen
                logIT(LOG_INFO, "New unit: %s", unit);
                uPtr = newUnitNode(uStartPtr);
                if (! uStartPtr) {
                    uStartPtr = uPtr;
                }
                uPtr->name = calloc(strlen(unit) + 1, sizeof(char));
                strcpy(uPtr->name, unit);
                unitFound = 1;
                prevPtr = cur;
                cur = cur->children;
                continue;
            }
        } else if (unitFound && strstr((char *)cur->name, "enum")) {
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"text");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (text)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                ePtr = newEnumNode(uPtr->ePtr);
                if (! uPtr->ePtr) {
                    uPtr->ePtr = ePtr;
                }
                ePtr->text = calloc(strlen(chrPtr) + 1, sizeof(char));
                strncpy(ePtr->text, chrPtr, strlen(chrPtr));
                chrPtr = getPropertyNode(cur->properties, (xmlChar *)"bytes");
                if (chrPtr) {
                    logIT(LOG_INFO, "          (%d) Node::Name=%s Type:%d Content=%s (bytes)",
                          cur->line, cur->name, cur->type, chrPtr);
                    bzero(string, sizeof(string));
                    ePtr->len = string2chr(chrPtr, string, sizeof(string));
                    ePtr->bytes = calloc(ePtr->len, sizeof(char));
                    memcpy(ePtr->bytes, string, ePtr->len);
                }
            } else {
                logIT(LOG_ERR, "Property node without text=");
                return NULL;
            }
            cur = getNextNode(cur, prevPtr);
        } else if (unitFound && strstr((char *)cur->name, "abbrev")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                uPtr->abbrev = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(uPtr->abbrev, chrPtr);
            } else {
                nullIT(&uPtr->abbrev);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (unitFound && (strcmp((char *)cur->name, "calc") == 0)) {
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"get");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (get)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                uPtr->gCalc = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(uPtr->gCalc, chrPtr);
            } else {
                nullIT(&uPtr->gCalc);
            }
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"set");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (set)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                uPtr->sCalc = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(uPtr->sCalc, chrPtr);
            } else {
                nullIT(&uPtr->sCalc);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (unitFound && (strcmp((char *)cur->name, "icalc") == 0)) {
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"get");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (get)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                uPtr->gICalc = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(uPtr->gICalc, chrPtr);
            } else {
                nullIT(&uPtr->gICalc);
            }
            chrPtr = getPropertyNode(cur->properties, (xmlChar *)"set");
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s (set)",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                uPtr->sICalc = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(uPtr->sICalc, chrPtr);
            } else {
                nullIT(&uPtr->sICalc);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (unitFound && strstr((char *)cur->name, "type")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                uPtr->type = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(uPtr->type, chrPtr);
            } else {
                nullIT(&uPtr->type);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (unitFound && strstr((char *)cur->name, "entity")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                uPtr->entity = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(uPtr->entity, chrPtr);
            } else {
                nullIT(&uPtr->entity);
            }
            cur = getNextNode(cur, prevPtr);
        } else {
            logIT(LOG_ERR, "Error parsing unit");
            return NULL;
        }
    }

    return uStartPtr;
}

static macroPtr parseMacro(xmlNodePtr cur)
{
    macroPtr mPtr;
    macroPtr mStartPtr = NULL;
    char *macro;
    char *chrPtr;
    int macroFound = 0;
    xmlNodePtr prevPtr;

    while (cur) {
        logIT(LOG_INFO, "MACRO: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }

        if (strstr((char *)cur->name, "macro")) {
            macro = getPropertyNode(cur->properties, (xmlChar *)"name");
            if (macro) {
                // Read new macro
                logIT(LOG_INFO, "New macro: %s", macro);
                mPtr = newMacroNode(mStartPtr);
                if (! mStartPtr) {
                    mStartPtr = mPtr;
                }
                mPtr->name = calloc(strlen(macro) + 1, sizeof(char));
                strcpy(mPtr->name, macro);
                macroFound = 1;
                prevPtr = cur;
                cur = cur->children;
                continue;
            }
        } else if (macroFound && strstr((char *)cur->name, "command")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                mPtr->command = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(mPtr->command, chrPtr);
            } else {
                nullIT(&mPtr->command);
            }
            cur = getNextNode(cur, prevPtr);
        } else {
            logIT(LOG_INFO, "Error parsing macro");
            return NULL;
        }
    }

    return mStartPtr;
}

static commandPtr parseCommand(xmlNodePtr cur, commandPtr cPtr, devicePtr dePtr)
{
    commandPtr cStartPtr = NULL;
    devicePtr dPtr;
    char *command;
    char *protocmd;
    char *chrPtr;
    xmlNodePtr prevPtr;
    char string[256];    // TODO: get rid of that one
    char *id;
    int commandFound;
    commandPtr ncPtr;
    short count;

    // We look for a recursive call (then, cPtr is set)
    if (! cPtr) {
        commandFound = 0;
    } else {
        commandFound = 1;
        prevPtr = NULL;
    }

    while (cur) {
        logIT(LOG_INFO, "COMMAND: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (xmlIsBlankNode(cur)) {
            cur = cur->next;
            continue;
        }

        if (strcmp((char *)cur->name, "command") == 0) {
            command = getPropertyNode(cur->properties, (xmlChar *)"name");
            protocmd = getPropertyNode(cur->properties, (xmlChar *)"protocmd");
            if (command) {
                // Read new command
                logIT(LOG_INFO, "New command: %s", command);
                cPtr = newCommandNode(cStartPtr);
                if (! cStartPtr) {
                    cStartPtr = cPtr;
                }
                cPtr->nodeType = 1; // No copy, we need this for deleting
                if (command) {
                    cPtr->name = calloc(strlen(command) + 1, sizeof(char));
                    strcpy(cPtr->name, command);
                } else {
                    nullIT(&cPtr->name);
                }
                if (protocmd) {
                    cPtr->pcmd = calloc(strlen(protocmd) + 1, sizeof(char));
                    strcpy(cPtr->pcmd, protocmd);
                } else {
                    nullIT(&cPtr->pcmd);
                }
                commandFound = 1;
                prevPtr = cur;
                cur = cur->children;
                continue;
            }
        } else if (commandFound && strstr((char *)cur->name, "device")) {
            id = getPropertyNode(cur->properties, (xmlChar *)"ID");
            protocmd = getPropertyNode(cur->properties, (xmlChar *)"protocmd");
            if (id) {
                // Read new device below command
                logIT(LOG_INFO, "    New device command: %s", id);
                // Search device from the list
                if (! (dPtr = getDeviceNode(dePtr, id))) {
                    logIT(LOG_ERR, "Device %s is not defined (%d)", id, cur->line);
                    return NULL;
                }
                // We take the description from the command entry
                strncpy(string, cPtr->description, sizeof(string));
                // Now again recursively parseCommand for all children
                ncPtr = newCommandNode(NULL);
                parseCommand(cur->children, ncPtr, dePtr);
                if (! dPtr->cmdPtr) {
                    dPtr->cmdPtr = ncPtr;
                } else {
                    addCommandNode(dPtr->cmdPtr, ncPtr);
                }
                // Refer to description in new accounts
                ncPtr->description = cPtr->description;
                ncPtr->name = cPtr->name;
                // If no unit has been given, we copy it
                if (! ncPtr->unit && cPtr->unit) {
                    ncPtr->unit = calloc(strlen(cPtr->unit) + 1, sizeof(char));
                    strcpy(ncPtr->unit, cPtr->unit);
                }
                // Same for the protocol command
                if (protocmd) {
                    ncPtr->pcmd = calloc(strlen(protocmd) + 1, sizeof(char));
                    strcpy(ncPtr->pcmd, protocmd);
                } else {
                    ncPtr->pcmd = calloc(strlen(cPtr->pcmd) + 1, sizeof(char));
                    strcpy(ncPtr->pcmd, cPtr->pcmd);
                }
                ncPtr->nodeType = 2; // 2 == decription, name has been copied
                cur = getNextNode(cur, prevPtr);
            }
        } else if (commandFound && strstr((char *)cur->name, "addr")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                cPtr->addr = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(cPtr->addr, chrPtr);
            } else {
                nullIT(&cPtr->addr);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (commandFound && strstr((char *)cur->name, "error")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                bzero(string, sizeof(string));
                if ((count = string2chr(chrPtr, string, sizeof(string)))) {
                    cPtr->errStr = calloc(count, sizeof(char));
                    memcpy(cPtr->errStr, string, count);
                }
            } else {
                nullIT(&cPtr->errStr);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (commandFound && strstr((char *)cur->name, "unit")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                cPtr->unit = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(cPtr->unit, chrPtr);
            } else {
                nullIT(&cPtr->unit);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (commandFound && (strcmp((char *)cur->name, "precommand") == 0)) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                cPtr->precmd = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(cPtr->precmd, chrPtr);
            } else {
                nullIT(&cPtr->precmd);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (commandFound && strstr((char *)cur->name, "description")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                cPtr->description = calloc(strlen(chrPtr) + 1, sizeof(char));
                strcpy(cPtr->description, chrPtr);
            } else {
                nullIT(&cPtr->description);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (commandFound && strstr((char *)cur->name, "len")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                cPtr->len = atoi(chrPtr);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (commandFound && strstr((char *)cur->name, "bit")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                cPtr->bit = atoi(chrPtr);
            }
            cur = getNextNode(cur, prevPtr);
        } else {
            logIT(LOG_ERR, "Error parsing command");
            return NULL;
        }
    }

    return cStartPtr;
}

icmdPtr parseICmd(xmlNodePtr cur)
{
    icmdPtr icPtr;
    icmdPtr icStartPtr = NULL;
    char *command;
    char *chrPtr;
    int commandFound = 0;
    xmlNodePtr prevPtr;

    while (cur) {
        logIT(LOG_INFO, "ICMD: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);
        if (xmlIsBlankNode(cur))  {
            //if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }
        if (strstr((char *)cur->name, "command")) {
            command = getPropertyNode(cur->properties, (xmlChar *)"name");
            if (command) {
                // Read new command
                logIT(LOG_INFO, "New iCommand: %s", command);
                icPtr = newIcmdNode(icStartPtr);
                if (! icStartPtr) {
                    icStartPtr = icPtr;
                }
                icPtr->name = calloc(strlen(command) + 1, sizeof(char));
                strcpy(icPtr->name, command);
                commandFound = 1;
                prevPtr = cur;
                cur = cur->children;
                continue;
            }
        } else if (commandFound && strstr((char *)cur->name, "send")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                icPtr->send = calloc(strlen(chrPtr) + 2, sizeof(char));
                strcpy(icPtr->send, chrPtr);
            } else {
                nullIT(&icPtr->send);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (commandFound && strstr((char *)cur->name, "retry")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                icPtr->retry = atoi(chrPtr);
            }
            cur = getNextNode(cur, prevPtr);
        } else if (commandFound && strstr((char *)cur->name, "recvTimeout")) {
            chrPtr = getTextNode(cur);
            logIT(LOG_INFO, "   (%d) Node::Name=%s Type:%d Content=%s",
                  cur->line, cur->name, cur->type, chrPtr);
            if (chrPtr) {
                icPtr->recvTimeout = atoi(chrPtr);
            }
            cur = getNextNode(cur, prevPtr);
        } else {
            logIT(LOG_ERR, "Error parsing command");
            return NULL;
        }
    }

    return icStartPtr;
}

devicePtr parseDevice(xmlNodePtr cur, protocolPtr pPtr)
{
    devicePtr dPtr;
    devicePtr dStartPtr = NULL;
    char *proto;
    char *name;
    char *id;
    xmlNodePtr prevPtr;

    while (cur) {
        logIT(LOG_INFO, "DEVICE: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }

        if (strstr((char *)cur->name, "device")) {
            name = getPropertyNode(cur->properties, (xmlChar *)"name");
            id = getPropertyNode(cur->properties, (xmlChar *)"ID");
            proto = getPropertyNode(cur->properties, (xmlChar *)"protocol");
            if (proto) {
                // Read new protocol
                logIT(LOG_INFO, "    Neues Device: name=%s ID=%s proto=%s", name, id, proto);
                dPtr = newDeviceNode(dStartPtr);
                if (! dStartPtr) {
                    dStartPtr = dPtr;
                } // Remember anchor
                if (name) {
                    dPtr->name = calloc(strlen(name) + 1, sizeof(char));
                    strcpy(dPtr->name, name);
                } else {
                    nullIT(&dPtr->name);
                }

                if (id) {
                    dPtr->id = calloc(strlen(id) + 1, sizeof(char));
                    strcpy(dPtr->id, id);
                } else {
                    nullIT(&dPtr->id);
                }

                if (! (dPtr->protoPtr = getProtocolNode(pPtr, proto))) {
                    logIT(LOG_ERR, "Protocol %s not defined", proto);
                    return NULL;
                }
            } else {
                logIT(LOG_ERR, "Error parsing device");
                return NULL;
            }

            prevPtr = cur;
            cur = cur->next;

        } else {
            cur = getNextNode(cur, prevPtr);
        }
    }

    return dStartPtr;
}

static protocolPtr parseProtocol(xmlNodePtr cur)
{
    int protoFound = 0;
    protocolPtr protoPtr;
    protocolPtr protoStartPtr = NULL;
    macroPtr mPtr;
    icmdPtr icPtr;
    char *proto;
    char *chrPtr;
    xmlNodePtr prevPtr;

    while (cur) {
        logIT(LOG_INFO, "PROT: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (cur->type == XML_TEXT_NODE) {
            cur = cur->next;
            continue;
        }

        if (strstr((char *)cur->name, "protocol")) {
            proto = getPropertyNode(cur->properties, (xmlChar *)"name");
            if (proto) {
                // Read new protocol
                logIT(LOG_INFO, "New protocol %s", proto);
                protoPtr = newProtocolNode(protoStartPtr);
                if (! protoStartPtr) {
                    protoStartPtr = protoPtr;
                } // Remember anchor
                if (proto) {
                    protoPtr->name = calloc(strlen(proto) + 1, sizeof(char));
                    strcpy(protoPtr->name, proto);
                } else {
                    nullIT(&protoPtr->name);
                }
            } else {
                logIT(LOG_ERR, "Error parsing protocol");
                return NULL;
            }
            protoFound = 1;
            prevPtr = cur;
            cur = cur->children;
        } else if (protoFound && strstr((char *)cur->name, "pid")) {
            chrPtr = getTextNode(cur);
            protoPtr->id = hex2chr(chrPtr);
            cur = getNextNode(cur, prevPtr);
        } else if (protoFound && strstr((const char *)cur->name, "macros")) {
            mPtr = parseMacro(cur->children);
            if (mPtr) {
                protoPtr->mPtr = mPtr;
            }
            cur = getNextNode(cur, prevPtr);
        } else if (protoFound && strstr((char *)cur->name, "commands")) {
            icPtr = parseICmd(cur->children);
            if (icPtr) {
                protoPtr->icPtr = icPtr;
            }
            cur = getNextNode(cur, prevPtr);
        } else {
            cur = getNextNode(cur, prevPtr);
        }
    }

    return protoStartPtr;
}

static void removeComments(xmlNodePtr node)
{
    while (node) {
        //printf("type:%d name=%s\n",node->type, node->name);
        if (node->children) {
            // if the node has children, process the children first
            removeComments(node->children);
        }
        if (node->type == XML_COMMENT_NODE) {
            // if the node is a comment?
            //printf("found comment\n");
            if (node->next) {
                // recurse to last comment node
                removeComments(node->next);
            }
            xmlUnlinkNode(node); // unlink
            xmlFreeNode(node);   // and free the node
        }
        node = node->next;
    }
}

// Search for the next node.
// forward, if the next node isn't a text node or the next node but one exists
// else backwards if prevPtr is given
// else return null.
static xmlNodePtr getNextNode(xmlNodePtr cur, xmlNodePtr prevPtr)
{
    if ((cur->next && (!(cur->next->type==XML_TEXT_NODE)||cur->next->next)))
    {
        return cur->next;
    }
    if (prevPtr != NULL)
    {
        return prevPtr->next;
    }
    return NULL;
}

void compileCommands()
{
    compileCommand(devPtr, uPtr);
}

int parseXMLFile(char *filename)
{
    xmlDocPtr doc;
    xmlNodePtr cur, curStart;
    xmlNodePtr prevPtr;
    xmlNsPtr ns;
    //char string[256];
    devicePtr dPtr;
    commandPtr cPtr, ncPtr;
    protocolPtr TprotoPtr = NULL;
    unitPtr TuPtr = NULL;
    devicePtr TdevPtr = NULL;
    commandPtr TcmdPtr = NULL;
    configPtr TcfgPtr = NULL;

    xmlKeepBlanksDefault(0);
    doc = xmlParseFile(filename);
    if (doc == NULL) {
        return 0;
    }
    curStart = xmlDocGetRootElement(doc);
    cur = curStart;
    if (cur == NULL) {
        logIT(LOG_ERR, "empty document\n");
        xmlFreeDoc(doc);
        return 0;
    }
    ns = xmlSearchNsByHref(doc, cur, (const xmlChar *) "http://www.openv.de/vcontrol");
    if (ns == NULL) {
        logIT(LOG_ERR, "document of the wrong type, vcontrol Namespace not found");
        xmlFreeDoc(doc);
        return 0;
    }
    if (xmlStrcmp(cur->name, (const xmlChar *) "V-Control")) {
        logIT(LOG_ERR, "document of the wrong type, root node != V-Control");
        xmlFreeDoc(doc);
        return 0;
    }
    // Run Xinlcude
    short xc = 0;
    if ((xc = xmlXIncludeProcessFlags(doc, XML_PARSE_XINCLUDE | XML_PARSE_NOXINCNODE)) == 0) {
        logIT(LOG_WARNING, "Didn't perform XInclude");
    } else if (xc < 0) {
        logIT(LOG_ERR, "Error during XInclude");
        return 0;
    } else {
        logIT(LOG_INFO, "%d XInclude performed", xc);
    }

    removeComments(cur); // now the xml tree is complete --> remove all comments

    int unixFound = 0;
    int protocolsFound = 0;
    cur = cur->children;

    while (cur) {
        logIT(LOG_INFO, "XML: (%d) Node::Name=%s Type:%d Content=%s",
              cur->line, cur->name, cur->type, cur->content);

        if (xmlIsBlankNode(cur)) {
            cur = cur->next;
            continue;
        }

        if (strstr((char *)cur->name, "unix"))  {
            if (unixFound) {
                // We must not reach here, second pass
                logIT(LOG_ERR, "Error in XML config");
                return 0;
            }
            prevPtr = cur;
            unixFound = 1;
            cur = cur->children; // On Unix, config follows. Thus children.
            continue;
        }

        if (strstr((char *)cur->name, "extern")) {
            cur = cur->children;
            if (cur && strstr((char *)cur->name, "vito"))  {
                prevPtr = cur;
                cur = cur->children; // The xinclude stuff can be found at <extern><vito>
                continue;
            } else {
                cur = prevPtr->next;
            }
        } else if (strstr((char *)cur->name, "protocols")) {
            if (protocolsFound) {
                // We must not reach here, second pass
                logIT(LOG_ERR, "Error in XML config");
                return 0;
            }
            protocolsFound = 1;
            if (! (TprotoPtr = parseProtocol(cur->children))) {
                return 0;
            }
            cur = cur->next; // Same level as Unix, proceed
            continue;
        } else if (strstr((char *)cur->name, "units")) {
            if (! (TuPtr = parseUnit(cur->children))) {
                return 0;
            }
            cur = cur->next; // Same level as Unix, proceed
            continue;
        } else if (strstr((char *)cur->name, "commands")) {
            if (! (TcmdPtr = parseCommand(cur->children, NULL, TdevPtr))) {
                return 0;
            }
            //(cur->next && cur->next->next) ? (cur=cur->next) : (cur=prevPtr->next);
            (cur->next) ? (cur = cur->next) : (cur = prevPtr->next);
            continue;
        } else if (strstr((char *)cur->name, "devices")) {
            if (! (TdevPtr = parseDevice(cur->children, TprotoPtr))) {
                return 0;
            }
            (cur->next) ? (cur = cur->next) : (cur = prevPtr->next);
            continue;
        } else if (strstr((char *)cur->name, "config")) {
            if (! (TcfgPtr = parseConfig(cur->children))) {
                return 0;
            }
            (cur->next && cur->next->next) ? (cur = cur->next) : (cur = prevPtr->next);
        } else {
            cur = cur->next;
        }
    }

    // For all commands that have default definitons,
    // we roam all devices and add the particular commands.
    cPtr = TcmdPtr;
    while (cPtr) {
        dPtr = TdevPtr;
        while (dPtr) {
            if (! getCommandNode(dPtr->cmdPtr, cPtr->name)) {
                // We don't know this one and copy the commands
                logIT(LOG_INFO, "Copying command %s to device %s", cPtr->name, dPtr->id);
                ncPtr = newCommandNode(dPtr->cmdPtr);
                if (! dPtr->cmdPtr) {
                    dPtr->cmdPtr = ncPtr;
                }
                ncPtr->name = cPtr->name;
                ncPtr->pcmd = cPtr->pcmd;
                ncPtr->addr = cPtr->addr;
                ncPtr->unit = cPtr->unit;
                ncPtr->bit = cPtr->bit;
                ncPtr->errStr = cPtr->errStr;
                ncPtr->precmd = cPtr->precmd;
                ncPtr->description = cPtr->description;
                ncPtr->len = cPtr->len;
            }
            dPtr = dPtr->next;
        }
        cPtr = cPtr->next;
    }

    // We search the default device
    if (! (TcfgPtr->devPtr = getDeviceNode(TdevPtr, TcfgPtr->devID))) {
        logIT(LOG_ERR, "Device %s is not defined\n", TcfgPtr->devID);
        return 0;
    }

    // If we reach here, the loading has been successful.
    // Noew, we free the old lists and allocate the new ones.

    // If we're called repetitive (SIGHUP), everything should be freed.
    freeAllLists();

    protoPtr = TprotoPtr;
    uPtr = TuPtr;
    devPtr = TdevPtr;
    cmdPtr = TcmdPtr;
    cfgPtr = TcfgPtr;

    xmlFreeDoc(doc);
    return 1;
}

static void freeAllLists()
{
    removeProtocolList(protoPtr);
    removeUnitList(uPtr);
    removeDeviceList(devPtr);
    removeCommandList(cmdPtr);
    protoPtr = NULL;
    uPtr = NULL;
    devPtr = NULL;
    cmdPtr = NULL;
    if (cfgPtr) {
        free(cfgPtr->tty);
        free(cfgPtr->logfile);
        free(cfgPtr->devID);
        removeAllowList(cfgPtr->aPtr);
        free(cfgPtr);
        cfgPtr = NULL;
    }
}

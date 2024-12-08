#ifndef ATTACK_TACTIC_H
#define ATTACK_TACTIC_H


struct attack_tactic
{
    const char id[8];
    const char name[24];
};

struct attack_tactic enterprise_tactics[] =
{
    { "TA0043", "Reconnaissance" },
    { "TA0042", "Resource Development" },
    { "TA0001", "Initial Access"},
    { "TA0002", "Execution"},
    { "TA0003", "Persistence"},
    { "TA0004", "Privilege Escalation"},
    { "TA0005", "Defense Evasion"},
    { "TA0006", "Credential Access"},
    { "TA0007", "Discovery"},
    { "TA0008", "Lateral Movement"},
    { "TA0009", "Collection"},
    { "TA0011", "Command and Control"},
    { "TA0010", "Exfiltration"},
    { "TA0040", "Impact"}
};

struct attack_technique
{
    //index to the enterprise_tactics array
    const char tactic;
    const char id[8];
    const char name[48];
    //somehow specify the "active" component here or have the "active" component point to this
};






#endif

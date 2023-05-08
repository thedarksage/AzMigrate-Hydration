#pragma once


typedef enum _DEVICE_PNP_STATE {

    NotStarted = 0,         // Not started yet
    Started,                // Device has received the START_DEVICE IRP
    StopPending,            // Device has received the QUERY_STOP IRP
    Stopped,                // Device has received the STOP_DEVICE IRP
    RemovePending,          // Device has received the QUERY_REMOVE IRP
    SurpriseRemovePending,  // Device has received the SURPRISE_REMOVE IRP
    Deleted,                 // Device has received the REMOVE_DEVICE IRP
    
    //Fix for bug 25726. Place this at the end.
    LimboState              //This can only  happen when our driver misses a PNP remove and devstack flattened thereof.
} DEVICE_PNP_STATE;


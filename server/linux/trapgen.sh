echo "1"
snmptrap -v 2c -c public sv1 "" Abhai-1-MIB-V1::trapSentinelNodeDown Abhai-1-MIB-V1::trapAmethystGuid s "5F25D74B-9C3D-4492-9CF3-EDE7F431A792--------------" Abhai-1-MIB-V1::trapHostGuid s "QWERTY4B-9C3D-4492-9CF3-EDE7F4315555--------------"

echo "2"
snmptrap -v 2c -c public sv1 "" Abhai-1-MIB-V1::trapOutpostAgentNodeDown Abhai-1-MIB-V1::trapAmethystGuid s "5F25D74B-9C3D-4492-9CF3-EDE7F431A792--------------" Abhai-1-MIB-V1::trapHostGuid s "QWERTY4B-9C3D-4492-9CF3-EDE7F4315555--------------"

echo "3"
snmptrap -v 2c -c public sv1 "" Abhai-1-MIB-V1::trapRPOSLAThresholdExceeded Abhai-1-MIB-V1::trapAmethystGuid s "5F25D74B-9C3D-4492-9CF3-EDE7F431A792--------------" Abhai-1-MIB-V1::trapHostGuid s "QWERTY4B-9C3D-4492-9CF3-EDE7F4315555--------------"

echo "4"
snmptrap -v 2c -c public sv1 "" Abhai-1-MIB-V1::trapSecondaryStorageFailure Abhai-1-MIB-V1::trapAmethystGuid s "5F25D74B-9C3D-4492-9CF3-EDE7F431A792--------------"

echo "5"
snmptrap -v 2c -c public sv1 "" Abhai-1-MIB-V1::trapSecondaryStorageUtilizationDangerLevel Abhai-1-MIB-V1::trapAmethystGuid s "5F25D74B-9C3D-4492-9CF3-EDE7F431A792--------------"

echo "6"
snmptrap -v 2c -c public sv1 "" Abhai-1-MIB-V1::trapSentinelResyncRequired Abhai-1-MIB-V1::trapAmethystGuid s "5F25D74B-9C3D-4492-9CF3-EDE7F431A792--------------" Abhai-1-MIB-V1::trapHostGuid s "QWERTY4B-9C3D-4492-9CF3-EDE7F4315555--------------" Abhai-1-MIB-V1::trapHostGuid s "ABCDEFGB-9C3D-4492-9CF3-EDE7F4315555--------------" Abhai-1-MIB-V1::trapVolumeGuid  s "VOLUME12-9C3D-4492-9CF3-EDE7F4315555--------------" Abhai-1-MIB-V1::trapVolumeGuid s "VOLUME34-9C3D-4492-9CF3-EDE7F4315555--------------" 

echo "7"
snmptrap -v 2c -c public sv1 "" Abhai-1-MIB-V1::trapReplicationInterfaceDown Abhai-1-MIB-V1::trapAmethystGuid s "5F25D74B-9C3D-4492-9CF3-EDE7F431A792--------------" Abhai-1-MIB-V1::trapPortStatus s "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" Abhai-1-MIB-V1::trapPortAttributes s "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"

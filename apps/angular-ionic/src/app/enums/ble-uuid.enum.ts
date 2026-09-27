export enum BleUuid {
  // 0000xxxx-0000-1000-8000-00805F9B34FB
  // UUIDs siehe im ESP Code: bleprph.h

  Service = '00001811-0000-1000-8000-00805f9b34fb',
  CustomService = '59462f12-9543-9999-12c8-58b459a2712d',

  CommandCharacteristic = '67e9fb07-1c9d-4f95-95f4-03fd45c285fd',
  QueryCharacteristic = '37594154-ea94-461a-b8f9-3e99899227fe',
}

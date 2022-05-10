<!-- markdownlint-disable MD013 -->
# OCPP Features

## Messages

### Core Profile

- :white_check_mark: Authorize
- :white_check_mark: BootNotification
- :white_check_mark: ChangeAvailability
- :white_check_mark: ChangeConfiguration
- :white_check_mark: ClearCache
- :no_entry: DataTransfer
- :white_check_mark: GetConfiguration
- :white_check_mark: Heartbeat
- :white_check_mark: MeterValues
- :white_check_mark: RemoteStartTransaction
- :white_check_mark: RemoteStopTransaction
- :white_check_mark: Reset
- :no_entry: StartTransaction
- :white_check_mark: StatusNotification
- :no_entry: StopTransaction
- :white_check_mark: UnlockConnector

### Firmware Management Profile

- :x: GetDiagnostics
- :x: DiagnosticsStatusNotification
- :x: FirmwareStatusNotification
- :x: UpdateFirmware

### Local Auth List Management Profile

- :x: GetLocalListVersion
- :x: SendLocalList

### Reservation Profile

- :x: CancelReservation
- :x: ReserveNow

### Smart Charging Profile

- :x: ClearChargingProfile
- :x: GetCompositeSchedule
- :x: SetChargingProfile

### Remote Trigger Profile

- :x: TriggerMessage

## Required configuration keys

### Core Profile

- :white_check_mark: AuthorizeRemoteTxRequests (type: boolean) (units: -)
- :white_check_mark: ClockAlignedDataInterval (type: int) (units: seconds)
- :white_check_mark: ConnectionTimeOut (type: int) (units: seconds)
- :white_check_mark: GetConfigurationMaxKeys (type: int) (units: -)
- :white_check_mark: HeartbeatInterval (type: int) (units: seconds)
- :white_check_mark: LocalAuthorizeOffline (type: boolean) (units: -)
- :white_check_mark: LocalPreAuthorize (type: boolean) (units: -)
- :white_check_mark: MeterValuesAlignedData (type: CSL) (units: -)
- :white_check_mark: MeterValuesSampledData (type: CSL) (units: -)
- :white_check_mark: MeterValueSampleInterval (type: int) (units: seconds)
- :white_check_mark: NumberOfConnectors (type: int) (units: -)
- :white_check_mark: ResetRetries (type: int) (units: times)
- :white_check_mark: ConnectorPhaseRotation (type: CSL) (units: -)
- :white_check_mark: StopTransactionOnEVSideDisconnect (type: boolean) (units: -)
- :white_check_mark: StopTransactionOnInvalidId (type: boolean) (units: -)
- :white_check_mark: StopTxnAlignedData (type: CSL) (units: -)
- :white_check_mark: StopTxnSampledData (type: CSL) (units: -)
- :white_check_mark: SupportedFeatureProfiles (type: CSL) (units: -)
- :white_check_mark: TransactionMessageAttempts (type: int) (units: times)
- :white_check_mark: TransactionMessageRetryInterval (type: int) (units: seconds)
- :white_check_mark: UnlockConnectorOnEVSideDisconnect (type: boolean) (units: -)

### Firmware Management Profile

- *none*

### Local Auth List Management Profile

- :x: LocalAuthListEnabled (type: boolean) (units: -)
- :x: LocalAuthListMaxLength (type: int) (units: -)
- :x: SendLocalListMaxLength (type: int) (units: -)

### Reservation Profile

- *none*

### Smart Charging Profile

- :x: ChargeProfileMaxStackLevel (type: int) (units: -)
- :x: ChargingScheduleAllowedChargingRateUnit (type: CSL) (units: -)
- :x: ChargingScheduleMaxPeriods (type: int) (units: -)
- :x: MaxChargingProfilesInstalled (type: int) (units: -)

### Remote Trigger Profile

- *none*

## Wallbox configuration

| Key | Value | Read-only |
|---|---|---|
| OCPPVersion | 1.6 | true |
| OCPPCentralAddress | <_Central System URL_> | false |
| StationName | <_Station Name_> | false |
| IntensityLimit | 8 | false |
| AuthorizationCacheEnabled | false | true |
| AuthorizeRemoteTxRequests | false | true |
| ClockAlignedDataInterval | 0 | true |
| ConnectionTimeOut | 180 | true |
| GetConfigurationMaxKeys | 0 | true |
| HeartbeatInterval | 180 | false |
| LocalAuthorizeOffline | false | true |
| LocalPreAuthorize | false | true |
| MeterValuesAlignedData | Energy.Active.Import.Register | true |
| MeterValuesSampledData | Energy.Active.Import.Register | true |
| MeterValueSampleInterval | 60 | true |
| NumberOfConnectors | 1 | true |
| ResetRetries | 0 | true |
| ConnectorPhaseRotation | NotApplicable | true |
| StopTransactionOnEVSideDisconnect | false | true |
| StopTransactionOnInvalidId | false | true |
| StopTxnAlignedData |  | true |
| StopTxnSampledData |  | true |
| SupportedFeatureProfiles | Core | true |
| TransactionMessageAttempts | 1 | true |
| TransactionMessageRetryInterval | 60 | true |
| UnlockConnectorOnEVSideDisconnect | true | true |

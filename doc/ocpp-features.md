<!-- markdownlint-disable MD013 -->
# OCPP Features

## Messages

### Core Profile

- :heavy_check_mark: Authorize
- :heavy_check_mark: BootNotification
- :heavy_check_mark: ChangeAvailability
- :heavy_check_mark: ChangeConfiguration
- :heavy_check_mark: ClearCache
- :no_entry: DataTransfer
- :heavy_check_mark: GetConfiguration
- :heavy_check_mark: Heartbeat
- :heavy_check_mark: MeterValues
- :heavy_check_mark: RemoteStartTransaction
- :heavy_check_mark: RemoteStopTransaction
- :heavy_check_mark: Reset
- :no_entry: StartTransaction
- :heavy_check_mark: StatusNotification
- :no_entry: StopTransaction
- :heavy_check_mark: UnlockConnector

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

- :heavy_check_mark: AuthorizeRemoteTxRequests (type: boolean) (units: -)
- :heavy_check_mark: ClockAlignedDataInterval (type: int) (units: seconds)
- :heavy_check_mark: ConnectionTimeOut (type: int) (units: seconds)
- :heavy_check_mark: GetConfigurationMaxKeys (type: int) (units: -)
- :heavy_check_mark: HeartbeatInterval (type: int) (units: seconds)
- :heavy_check_mark: LocalAuthorizeOffline (type: boolean) (units: -)
- :heavy_check_mark: LocalPreAuthorize (type: boolean) (units: -)
- :heavy_check_mark: MeterValuesAlignedData (type: CSL) (units: -)
- :heavy_check_mark: MeterValuesSampledData (type: CSL) (units: -)
- :heavy_check_mark: MeterValueSampleInterval (type: int) (units: seconds)
- :heavy_check_mark: NumberOfConnectors (type: int) (units: -)
- :heavy_check_mark: ResetRetries (type: int) (units: times)
- :heavy_check_mark: ConnectorPhaseRotation (type: CSL) (units: -)
- :interrobang: StopTransactionOnEVSideDisconnect (type: boolean) (units: -)
- :interrobang: StopTransactionOnInvalidId (type: boolean) (units: -)
- :heavy_check_mark: StopTxnAlignedData (type: CSL) (units: -)
- :heavy_check_mark: StopTxnSampledData (type: CSL) (units: -)
- :heavy_check_mark: SupportedFeatureProfiles (type: CSL) (units: -)
- :interrobang: TransactionMessageAttempts (type: int) (units: times)
- :interrobang: TransactionMessageRetryInterval (type: int) (units: seconds)
- :heavy_check_mark: UnlockConnectorOnEVSideDisconnect (type: boolean) (units: -)

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
| GetConfigurationMaxKeys | 32 | true |
| HeartbeatInterval | 180 | false |
| LocalAuthorizeOffline | false | true |
| LocalPreAuthorize | false | true |
| MeterValuesAlignedData | Energy.Active.Import.Register | true |
| MeterValuesSampledData | Energy.Active.Import.Register | true |
| MeterValueSampleInterval | 60 | true |
| NumberOfConnectors | 1 | true |
| ResetRetries | 0 | true |
| ConnectorPhaseRotation | 1.NotApplicable | true |
| ConnectorPhaseRotationMaxLength | 1 | true |
| StopTransactionOnEVSideDisconnect | true | true |
| StopTransactionOnInvalidId | true | true |
| StopTxnAlignedData |  | true |
| StopTxnAlignedDataMaxLength | 0 | true |
| StopTxnSampledData |  | true |
| StopTxnSampledDataMaxLength | 0 | true |
| SupportedFeatureProfiles | Core | true |
| TransactionMessageAttempts | 10 | true |
| TransactionMessageRetryInterval | 60 | true |
| UnlockConnectorOnEVSideDisconnect | true | true |

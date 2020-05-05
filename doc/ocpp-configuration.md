# OCPP Configuration

## Required configuration keys by feature profile

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

## Example

| Key | Value | Read-only |
|---|---|---|
| operatingmode | 2 | false |
| clustermanagement | 0 | true |
| simplifiedmode3 | true | false |
| ventilation | true | false |
| localisation | true | false |
| cpwratestep | 0 | true |
| isolatedinput1 | 0 | false |
| isolatedinput2 | 0 | false |
| sm3currentlowthreshold | 1 | true |
| sm3stopdelay | 3600 | true |
| postponecharge | false | false |
| allowpluggedcable | true | true |
| enableevdetection | true | false |
| schukowithnodetectionendofcharge | 60 | true |
| updatesetpointperiodinsec | 15 | true |
| remotesetpointperiodinsec | 5 | true |
| stationnetworktype | 3 | false |
| dhcp | 0 | false |
| strictdhcpmode | 0 | false |
| slavehaspublicip | 1 | false |
| stationname | 1A 01234 01 234 567 | false |
| dhcpmode | 1 | true |
| loadsheddingsetpoint | 0 | false |
| voltagererefence | 400 | false |
| currentpb1 | 32 | true |
| nbphase | 3 | true |
| maxintensitysocket | 32 | false |
| maxintensitystation | 64 | false |
| maxtevalue | 14 | false |
| headmetertype | 5 | false |
| headmeterprotocol | 1 | false |
| headmeterrtuaddress | 1 | false |
| headmetergatewayaddress | 130 | false |
| headmeterphase | 1 | false |
| stationmetertype | 5 | false |
| stationmeterprotocol | 1 | false |
| stationmeterrtuaddress | 2 | false |
| stationmetergatewayaddress | 130 | false |
| stationmeterphase | 3 | false |
| alternativemetertype | 5 | false |
| alternativemeterprotocol | 1 | false |
| alternativemeterrtuaddress | 3 | false |
| alternativemetergatewayaddress | 130 | false |
| alternativemeterphase | 3 | false |
| meteringpollingperiod | 1000 | false |
| powermeterphasesconnection | 1 | true |
| terminalphasesconnection | 1 | false |
| currentchargelogperiod | 60 | false |
| enablesuspendchargebybutton | false | false |
| useautotimemanagment | true | false |
| timeservername | pool.ntp.org | false |
| timezone | UTC | false |
| pulsetoenergyfactor | 1 | false |
| degradedmodesetpointmono | 8 | false |
| degradedmodesetpointtri | 14 | false |
| monophasedloadsheddingfloorvalue | 8 | false |
| triphasedloadsheddingfloorvalue | 14 | false |
| emsetting | 3 | false |
| upstreamprothightrshld | 100 | true |
| homeupstreamprotection | 32 | true |
| staticmaxintensitycluster | 64 | false |
| loadsheddingpriority | 0 | false |
| loadsheddingperiod | 900 | false |
| phaserotation | false | false |
| imaxstation | 64 | true |
| overloaddelay | 90 | true |
| overloadtolerance | 18 | true |
| AllowOfflineTxForUnknownId | true | false |
| authenticationmanager | 2 | false |
| rfidstatustimeout | 10 | false |
| masterkeyavailability | false | false |
| authenticationmode | 1 | false |
| supervisionsystem | 2 | false |
| controlchargebyremotecommand | true | false |
| ocppversion | 1.6 | false |
| ocppcentraladdress | wss://chargebox.server.example.com/OCPP16/dfa7453686e48956e49f8a96 | false |
| ocppboxlocalport | 8080 | false |
| ocppboxpublicip | 192.168.1.123 | false |
| ocppboxpublicport | 8080 | false |
| ocppboxlocalssl | 0 | true |
| ocppboxaddressreplyto | <http://www.w3.org/2005/08/addressing/anonymous> | false |
| ocppmodemipaddress | 192.168.1.254 | true |
| ocppmodempresence | 0 | true |
| modemtype | undefined | true |
| confmodemretryinterval | 0 | false |
| boxidentity | Charger-01 | false |
| defaultidtag | UNDEFINED | false |
| metervaluesaligneddata | Energy.Active.Import.Register | false |
| metervaluessampleddata | Energy.Active.Import.Register | false |
| metervaluesampleinterval | 60 | false |
| clockaligneddatainterval | 0 | false |
| ocppconnecttimeout | 60 | false |
| websocketpinginterval | 120 | false |
| minimumstatusduration | 0 | false |
| transactionmessageretryinterval | 60 | false |
| transactionmessageattempts | 500 | false |
| truncatebootnotificationsserialnumbers | false | false |
| clienthttpsoptions | DEFAULT | true |
| AuthorizationCacheEnabled | false | false |
| enableplugnumbering | false | false |
| compressdiagnostic | 1 | false |
| ocppemdegradedmodeenabled | 0 | false |
| HeartBeatInterval | 180 | false |
| AuthorizeRemoteTxRequests | false | true |
| ChargeProfileMaxStackLevel | 999 | true |
| ChargingScheduleAllowedChargingRateUnit | Current | true |
| ChargingScheduleMaxPeriods | 50 | true |
| ConnectionTimeOut | 30 | true |
| ConnectorPhaseRotation | 1.Unknown, 2.Unknown | true |
| ConnectorPhaseRotationMaxLength | 2 | true |
| ConnectorSwitch3to1PhaseSupported | false | true |
| GetConfigurationMaxKeys | 200 | true |
| MaxChargingProfilesInstalled | 99 | true |
| NumberOfConnectors | 2 | true |
| ReserveConnectorZeroSupported | false | true |
| ResetRetries | 0 | true |
| StopTransactionOnEVSideDisconnect | true | true |
| StopTransactionOnInvalidId | true | true |
| StopTxnAlignedData | undefined | true |
| StopTxnAlignedDataMaxLength | 0 | true |
| StopTxnSampledData | undefined | true |
| StopTxnSampledDataMaxLength | 0 | true |
| SupportedFeatureProfiles | Core,FirmwareManagement,Reservation,SmartCharging | true |
| SupportedFeatureProfilesMaxLength | 4 | true |
| UnlockConnectorOnEVSideDisconnect | true | true |

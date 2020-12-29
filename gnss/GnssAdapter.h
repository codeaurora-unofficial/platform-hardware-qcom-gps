/* Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation, nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef GNSS_ADAPTER_H
#define GNSS_ADAPTER_H

#include <LocAdapterBase.h>
#include <LocDualContext.h>
#include <IOsObserver.h>
#include <EngineHubProxyBase.h>
#include <LocationAPI.h>
#include <Agps.h>
#include <SystemStatus.h>
#include <XtraSystemStatusObserver.h>

#define MAX_URL_LEN 256
#define NMEA_SENTENCE_MAX_LENGTH 200
#define GLONASS_SV_ID_OFFSET 64
#define MAX_SATELLITES_IN_USE 12
#define LOC_NI_NO_RESPONSE_TIME 20
#define LOC_GPS_NI_RESPONSE_IGNORE 4
#define ODCPI_EXPECTED_INJECTION_TIME_MS 10000

class GnssAdapter;

class OdcpiTimer : public LocTimer {
public:
    OdcpiTimer(GnssAdapter* adapter) :
            LocTimer(), mAdapter(adapter), mActive(false) {}

    inline void start() {
        mActive = true;
        LocTimer::start(ODCPI_EXPECTED_INJECTION_TIME_MS, false);
    }
    inline void stop() {
        mActive = false;
        LocTimer::stop();
    }
    inline void restart() {
        stop();
        start();
    }
    inline bool isActive() {
        return mActive;
    }

private:
    // Override
    virtual void timeOutCallback() override;

    GnssAdapter* mAdapter;
    bool mActive;
};

typedef struct {
    pthread_t               thread;        /* NI thread */
    uint32_t                respTimeLeft;  /* examine time for NI response */
    bool                    respRecvd;     /* NI User reponse received or not from Java layer*/
    void*                   rawRequest;
    uint32_t                reqID;         /* ID to check against response */
    GnssNiResponse          resp;
    pthread_cond_t          tCond;
    pthread_mutex_t         tLock;
    GnssAdapter*            adapter;
} NiSession;
typedef struct {
    NiSession session;    /* SUPL NI Session */
    NiSession sessionEs;  /* Emergency SUPL NI Session */
    uint32_t reqIDCounter;
} NiData;

typedef enum {
    NMEA_PROVIDER_AP = 0, // Application Processor Provider of NMEA
    NMEA_PROVIDER_MP      // Modem Processor Provider of NMEA
} NmeaProviderType;
typedef struct {
    GnssSvType svType;
    const char* talker;
    uint64_t mask;
    uint32_t svIdOffset;
} NmeaSvMeta;

typedef struct {
    double latitude;
    double longitude;
    float  accuracy;
    // the CPI will be blocked until the boot time
    // specified in blockedTillTsMs
    int64_t blockedTillTsMs;
    // CPIs whose both latitude and longitude differ
    // no more than latLonThreshold will be blocked
    // in units of degree
    double latLonDiffThreshold;
} BlockCPIInfo;

typedef struct {
    bool isValid;
    bool enable;
    float tuncThresholdMs; // need to be specified if enable is true
    uint32_t energyBudget; // need to be specified if enable is true
} TuncConfigInfo;

typedef struct {
    bool isValid;
    bool enable;
} PaceConfigInfo;

typedef struct {
    bool isValid;
    bool enable;
    bool enableFor911;
} RobustLocationConfigInfo;

typedef struct {
    TuncConfigInfo tuncConfigInfo;
    PaceConfigInfo paceConfigInfo;
    RobustLocationConfigInfo robustLocationConfigInfo;
} LocIntegrationConfigInfo;

using namespace loc_core;

namespace loc_core {
    class SystemStatus;
}

typedef std::function<void(
    uint64_t gnssEnergyConsumedFromFirstBoot
)> GnssEnergyConsumedCallback;

typedef void (*removeClientCompleteCallback)(LocationAPI* client);

typedef void* QDgnssListenerHDL;
typedef std::function<void(
    bool    sessionActive
)> QDgnssSessionActiveCb;

struct CdfwInterface {
    QDgnssListenerHDL (*createUsableReporter)(
            QDgnssSessionActiveCb sessionActiveCb);

    void (*destroyUsableReporter)(QDgnssListenerHDL handle);

    void (*reportUsable)(QDgnssListenerHDL handle, bool usable);
};

class GnssAdapter : public LocAdapterBase {

    /* ==== Engine Hub ===================================================================== */
    EngineHubProxyBase* mEngHubProxy;
    bool mNHzNeeded;
    bool mSPEAlreadyRunningAtHighestInterval;

    /* ==== CLIENT ========================================================================= */
    typedef std::map<LocationAPI*, LocationCallbacks> ClientDataMap;
    ClientDataMap mClientData;

    /* ==== TRACKING ======================================================================= */
    TrackingOptionsMap mTrackingSessions;
    LocPosMode mLocPositionMode;
    GnssSvUsedInPosition mGnssSvIdUsedInPosition;
    bool mGnssSvIdUsedInPosAvail;
    GnssSvMbUsedInPosition mGnssMbSvIdUsedInPosition;
    bool mGnssMbSvIdUsedInPosAvail;

    /* ==== CONTROL ======================================================================== */
    LocationControlCallbacks mControlCallbacks;
    uint32_t mPowerVoteId;
    uint32_t mNmeaMask;
    uint64_t mPrevNmeaRptTimeNsec;
    GnssSvIdConfig mGnssSvIdConfig;
    GnssSvTypeConfig mGnssSeconaryBandConfig;
    GnssSvTypeConfig mGnssSvTypeConfig;
    GnssSvTypeConfigCallback mGnssSvTypeConfigCb;
    LocIntegrationConfigInfo mLocConfigInfo;

    /* ==== NI ============================================================================= */
    NiData mNiData;

    /* ==== AGPS =========================================================================== */
    // This must be initialized via initAgps()
    AgpsManager mAgpsManager;
    AgpsCbInfo mAgpsCbInfo;
    void initAgps(const AgpsCbInfo& cbInfo);

    /* ==== DGNSS Data Usable Report======================================================== */
    QDgnssListenerHDL mQDgnssListenerHDL;
    const CdfwInterface* mCdfwInterface;
    bool mDGnssNeedReport;
    bool mDGnssDataUsage;
    void reportDGnssDataUsable(GnssSvMeasurementSet &svMeasurementSet);

    /* ==== ODCPI ========================================================================== */
    OdcpiRequestCallback mOdcpiRequestCb;
    bool mOdcpiRequestActive;
    OdcpiTimer mOdcpiTimer;
    OdcpiRequestInfo mOdcpiRequest;
    void odcpiTimerExpire();

    /* === SystemStatus ===================================================================== */
    SystemStatus* mSystemStatus;
    std::string mServerUrl;
    std::string mMoServerUrl;
    XtraSystemStatusObserver mXtraObserver;
    LocationSystemInfo mLocSystemInfo;
    std::vector<GnssSvIdSource> mBlacklistedSvIds;
    PowerStateType mPowerState;

    /* === Misc ===================================================================== */
    BlockCPIInfo mBlockCPIInfo;

    /* === Misc callback from QMI LOC API ============================================== */
    GnssEnergyConsumedCallback mGnssEnergyConsumedCb;

    /*==== CONVERSION ===================================================================*/
    static void convertOptions(LocPosMode& out, const TrackingOptions& trackingOptions);
    static void convertLocation(Location& out, const UlpLocation& ulpLocation,
                                const GpsLocationExtended& locationExtended,
                                const LocPosTechMask techMask);
    static void convertLocationInfo(GnssLocationInfoNotification& out,
                                    const GpsLocationExtended& locationExtended,
                                    loc_sess_status status);
    static uint16_t getNumSvUsed(uint64_t svUsedIdsMask,
                                 int totalSvCntInThisConstellation);

    /* ======== UTILITIES ================================================================== */
    inline void initOdcpi(const OdcpiRequestCallback& callback);
    inline void injectOdcpi(const Location& location);
    inline void setNmeaReportRateConfig();

public:

    GnssAdapter();
    virtual inline ~GnssAdapter() { }

    /* ==== SSR ============================================================================ */
    /* ======== EVENTS ====(Called from QMI Thread)========================================= */
    virtual void handleEngineUpEvent();
    /* ======== UTILITIES ================================================================== */
    void restartSessions(bool modemSSR = false);
    void checkAndRestartSPESession();
    void suspendSessions();

    /* ==== CLIENT ========================================================================= */
    /* ======== COMMANDS ====(Called from Client Thread)==================================== */
    void addClientCommand(LocationAPI* client, const LocationCallbacks& callbacks);
    void removeClientCommand(LocationAPI* client,
                             removeClientCompleteCallback rmClientCb);
    void requestCapabilitiesCommand(LocationAPI* client);
    /* ======== UTILITIES ================================================================== */
    void saveClient(LocationAPI* client, const LocationCallbacks& callbacks);
    void eraseClient(LocationAPI* client);
    void notifyClientOfCachedLocationSystemInfo(LocationAPI* client,
                                                const LocationCallbacks& callbacks);
    void updateClientsEventMask();
    void stopClientSessions(LocationAPI* client);
    LocationCallbacks getClientCallbacks(LocationAPI* client);
    LocationCapabilitiesMask getCapabilities();
    inline PowerStateType getPowerState() { return mPowerState; }

    void broadcastCapabilities(LocationCapabilitiesMask);
    void setSuplHostServer(const char* server, int port, LocServerType type);

    /* ==== TRACKING ======================================================================= */
    /* ======== COMMANDS ====(Called from Client Thread)==================================== */
    uint32_t startTrackingCommand(
            LocationAPI* client, TrackingOptions& trackingOptions);
    void updateTrackingOptionsCommand(
            LocationAPI* client, uint32_t id, TrackingOptions& trackingOptions);
    void stopTrackingCommand(LocationAPI* client, uint32_t id);
    virtual void setPositionModeCommand(LocPosMode& locPosMode);
    /* ======== RESPONSES ================================================================== */
    void reportResponse(LocationAPI* client, LocationError err, uint32_t sessionId);
    /* ======== UTILITIES ================================================================== */
    bool hasCallbacksToStartTracking(LocationAPI* client);
    bool isTrackingSession(LocationAPI* client, uint32_t sessionId);
    void saveTrackingSession(LocationAPI* client, uint32_t sessionId,
                             const TrackingOptions& trackingOptions);
    void eraseTrackingSession(LocationAPI* client, uint32_t sessionId);

    bool setLocPositionMode(const LocPosMode& mode);
    LocPosMode& getLocPositionMode() { return mLocPositionMode; }

    bool startTrackingMultiplex(LocationAPI* client, uint32_t sessionId,
                                const TrackingOptions& trackingOptions);
    void startTracking(LocationAPI* client, uint32_t sessionId,
                       const TrackingOptions& trackingOptions);
    bool stopTrackingMultiplex(LocationAPI* client, uint32_t id);
    void stopTracking(LocationAPI* client, uint32_t id);
    bool updateTrackingMultiplex(LocationAPI* client, uint32_t id,
                                 const TrackingOptions& trackingOptions);
    void updateTracking(LocationAPI* client, uint32_t sessionId,
        const TrackingOptions& updatedOptions, const TrackingOptions& oldOptions);
    bool checkAndSetSPEToRunforNHz(LocPosMode & out);

    void setConstrainedTunc(bool enable, float tuncConstraint,
                            uint32_t energyBudget, uint32_t sessionId);
    void setPositionAssistedClockEstimator(bool enable, uint32_t sessionId);
    void gnssUpdateSvConfig(uint32_t sessionId,
                        const GnssSvTypeConfig& constellationEnablementConfig,
                        const GnssSvIdConfig& blacklistSvConfig);

    void gnssUpdateSecondaryBandConfig(
        uint32_t sessionId, const GnssSvTypeConfig& secondaryBandConfig);
    void gnssGetSecondaryBandConfig(uint32_t sessionId);
    void resetSvConfig(uint32_t sessionId);
    void configLeverArm(uint32_t sessionId, const LeverArmConfigInfo& configInfo);
    void configRobustLocation(uint32_t sessionId, bool enable, bool enableForE911);
    void configMinGpsWeek(uint32_t sessionId, uint16_t minGpsWeek);

    /* ==== NI ============================================================================= */
    /* ======== COMMANDS ====(Called from Client Thread)==================================== */
    void gnssNiResponseCommand(LocationAPI* client, uint32_t id, GnssNiResponse response);
    /* ======================(Called from NI Thread)======================================== */
    void gnssNiResponseCommand(GnssNiResponse response, void* rawRequest);
    /* ======== UTILITIES ================================================================== */
    bool hasNiNotifyCallback(LocationAPI* client);
    NiData& getNiData() { return mNiData; }

    /* ==== CONTROL CLIENT ================================================================= */
    /* ======== COMMANDS ====(Called from Client Thread)==================================== */
    uint32_t enableCommand(LocationTechnologyType techType);
    void disableCommand(uint32_t id);
    void setControlCallbacksCommand(LocationControlCallbacks& controlCallbacks);
    void readConfigCommand();
    void setConfigCommand();
    void requestUlpCommand();
    void initEngHubProxyCommand();
    uint32_t* gnssUpdateConfigCommand(const GnssConfig& config);
    uint32_t* gnssGetConfigCommand(GnssConfigFlagsMask mask);
    uint32_t gnssDeleteAidingDataCommand(GnssAidingData& data);
    void deleteAidingData(const GnssAidingData &data, uint32_t sessionId);
    void gnssUpdateXtraThrottleCommand(const bool enabled);
    std::vector<LocationError> gnssUpdateConfig(const std::string& oldServerUrl,
            const std::string& oldMoServerUrl, const GnssConfig& gnssConfigRequested,
            const GnssConfig& gnssConfigNeedEngineUpdate, size_t count = 0);

    /* ==== GNSS SV TYPE CONFIG ============================================================ */
    /* ==== COMMANDS ====(Called from Client Thread)======================================== */
    /* ==== These commands are received directly from client bypassing Location API ======== */
    void gnssUpdateSvTypeConfigCommand(GnssSvTypeConfig config);
    void gnssGetSvTypeConfigCommand(GnssSvTypeConfigCallback callback);
    void gnssResetSvTypeConfigCommand();

    /* ==== UTILITIES ====================================================================== */
    LocationError gnssSvIdConfigUpdateSync(const std::vector<GnssSvIdSource>& blacklistedSvIds);
    LocationError gnssSvIdConfigUpdateSync();
    void gnssSvIdConfigUpdate(const std::vector<GnssSvIdSource>& blacklistedSvIds);
    void gnssSvIdConfigUpdate();
    void gnssSvTypeConfigUpdate(const GnssSvTypeConfig& config);
    void gnssSvTypeConfigUpdate(bool sendReset = false);
    inline void gnssSetSvTypeConfig(const GnssSvTypeConfig& config)
    { mGnssSvTypeConfig = config; }
    inline void gnssSetSvTypeConfigCallback(GnssSvTypeConfigCallback callback)
    { mGnssSvTypeConfigCb = callback; }
    inline GnssSvTypeConfigCallback gnssGetSvTypeConfigCallback()
    { return mGnssSvTypeConfigCb; }
    void gnssSecondaryBandConfigUpdate(LocApiResponse* locApiResponse= nullptr);

    /* ========= AGPS ====================================================================== */
    /* ======== COMMANDS ====(Called from Client Thread)==================================== */
    void initDefaultAgpsCommand();
    void initAgpsCommand(const AgpsCbInfo& cbInfo);
    void dataConnOpenCommand(AGpsExtType agpsType,
            const char* apnName, int apnLen, AGpsBearerType bearerType);
    void dataConnClosedCommand(AGpsExtType agpsType);
    void dataConnFailedCommand(AGpsExtType agpsType);
    void getGnssEnergyConsumedCommand(GnssEnergyConsumedCallback energyConsumedCb);
    uint32_t setConstrainedTuncCommand (bool enable, float tuncConstraint,
                                        uint32_t energyBudget);
    uint32_t setPositionAssistedClockEstimatorCommand (bool enable);
    uint32_t gnssUpdateSvConfigCommand(const GnssSvTypeConfig& constellationEnablementConfig,
                                       const GnssSvIdConfig& blacklistSvConfig);
    uint32_t gnssUpdateSecondaryBandConfigCommand(
                                       const GnssSvTypeConfig& secondaryBandConfig);
    uint32_t gnssGetSecondaryBandConfigCommand();
    uint32_t configLeverArmCommand(const LeverArmConfigInfo& configInfo);
    uint32_t configRobustLocationCommand(bool enable, bool enableForE911);
    uint32_t configMinGpsWeekCommand(uint16_t minGpsWeek);
    uint32_t configDeadReckoningEngineParamsCommand(const DeadReckoningEngineConfig& dreConfig);
    uint32_t configOutputNmeaTypesCommand(GnssNmeaTypesMask enabledNmeaTypes);

    /* ========= ODCPI ===================================================================== */
    /* ======== COMMANDS ====(Called from Client Thread)==================================== */
    void initOdcpiCommand(const OdcpiRequestCallback& callback);
    void injectOdcpiCommand(const Location& location);
    /* ======== RESPONSES ================================================================== */
    void reportResponse(LocationError err, uint32_t sessionId);
    void reportResponse(size_t count, LocationError* errs, uint32_t* ids);
    /* ======== UTILITIES ================================================================== */
    LocationControlCallbacks& getControlCallbacks() { return mControlCallbacks; }
    void setControlCallbacks(const LocationControlCallbacks& controlCallbacks)
    { mControlCallbacks = controlCallbacks; }
    void setPowerVoteId(uint32_t id) { mPowerVoteId = id; }
    uint32_t getPowerVoteId() { return mPowerVoteId; }
    virtual bool isInSession() { return !mTrackingSessions.empty(); }
    void initDefaultAgps();
    bool initEngHubProxy();
    void initDGnssUsableReporter();
    void odcpiTimerExpireEvent();

    /* ==== REPORTS ======================================================================== */
    /* ======== EVENTS ====(Called from QMI/EngineHub Thread)===================================== */
    virtual void reportPositionEvent(const UlpLocation& ulpLocation,
                                     const GpsLocationExtended& locationExtended,
                                     enum loc_sess_status status,
                                     LocPosTechMask techMask,
                                     GnssDataNotification* pDataNotify = nullptr,
                                     int msInWeek = -1);
    virtual void reportEnginePositionsEvent(unsigned int count,
                                            EngineLocationInfo* locationArr);

    virtual void reportSvEvent(const GnssSvNotification& svNotify,
                               bool fromEngineHub=false);
    virtual void reportNmeaEvent(const char* nmea, size_t length);
    virtual void reportDataEvent(const GnssDataNotification& dataNotify, int msInWeek);
    virtual bool requestNiNotifyEvent(const GnssNiNotification& notify, const void* data);
    virtual void reportGnssMeasurementDataEvent(const GnssMeasurementsNotification& measurements,
                                                int msInWeek);
    virtual void reportSvMeasurementEvent(GnssSvMeasurementSet &svMeasurementSet);
    virtual void reportSvPolynomialEvent(GnssSvPolynomial &svPolynomial);
    virtual void reportSvEphemerisEvent(GnssSvEphemerisReport & svEphemeris);
    virtual void reportGnssSvIdConfigEvent(const GnssSvIdConfig& config);
    virtual void reportGnssSvTypeConfigEvent(const GnssSvTypeConfig& config);
    virtual void reportGnssConfigEvent(uint32_t sessionId, const GnssConfig& gnssConfig);
    virtual bool reportGnssEngEnergyConsumedEvent(uint64_t energyConsumedSinceFirstBoot);
    virtual void reportLocationSystemInfoEvent(const LocationSystemInfo& locationSystemInfo);

    virtual bool requestATL(int connHandle, LocAGpsType agps_type, LocApnTypeMask apn_type_mask);
    virtual bool releaseATL(int connHandle);
    virtual bool requestOdcpiEvent(OdcpiRequestInfo& request);
    virtual bool reportDeleteAidingDataEvent(GnssAidingData& aidingData);
    virtual bool reportKlobucharIonoModelEvent(GnssKlobucharIonoModel& ionoModel);
    virtual bool reportGnssAdditionalSystemInfoEvent(
            GnssAdditionalSystemInfo& additionalSystemInfo);

    /* ======== UTILITIES ================================================================= */
    bool needReport(const UlpLocation& ulpLocation,
            enum loc_sess_status status, LocPosTechMask techMask);
    bool needToGenerateNmeaReport(const uint32_t &gpsTimeOfWeekMs,
        const struct timespec32_t &apTimeStamp);
    void reportPosition(const UlpLocation &ulpLocation,
                        const GpsLocationExtended &locationExtended,
                        enum loc_sess_status status,
                        LocPosTechMask techMask);
    void reportEnginePositions(unsigned int count,
                               const EngineLocationInfo* locationArr);
    void reportSv(GnssSvNotification& svNotify);
    void reportNmea(const char* nmea, size_t length);
    void reportData(GnssDataNotification& dataNotify);
    bool requestNiNotify(const GnssNiNotification& notify, const void* data);
    void reportGnssMeasurementData(const GnssMeasurementsNotification& measurements);
    void reportGnssSvIdConfig(const GnssSvIdConfig& config);
    void reportGnssSvTypeConfig(const GnssSvTypeConfig& config);
    void reportGnssConfig(uint32_t sessionId, const GnssConfig& gnssConfig);
    void requestOdcpi(const OdcpiRequestInfo& request);
    void invokeGnssEnergyConsumedCallback(uint64_t energyConsumedSinceFirstBoot);
    void saveGnssEnergyConsumedCallback(GnssEnergyConsumedCallback energyConsumedCb);
    void reportLocationSystemInfo(const LocationSystemInfo & locationSystemInfo);
    void updatePowerState(PowerStateType powerState);

    /*======== GNSSDEBUG ================================================================*/
    bool getDebugReport(GnssDebugReport& report);
    /* get AGC information from system status and fill it */
    void getAgcInformation(GnssMeasurementsNotification& measurements, int msInWeek);
    /* get Data information from system status and fill it */
    void getDataInformation(GnssDataNotification& data, int msInWeek);

    /*==== SYSTEM STATUS ================================================================*/
    inline SystemStatus* getSystemStatus(void) { return mSystemStatus; }
    std::string& getServerUrl(void) { return mServerUrl; }
    std::string& getMoServerUrl(void) { return mMoServerUrl; }

    /*==== CONVERSION ===================================================================*/
    static uint32_t convertSuplVersion(const GnssConfigSuplVersion suplVersion);
    static uint32_t convertLppProfile(const GnssConfigLppProfile lppProfile);
    static uint32_t convertEP4ES(const GnssConfigEmergencyPdnForEmergencySupl);
    static uint32_t convertSuplEs(const GnssConfigSuplEmergencyServices suplEmergencyServices);
    static uint32_t convertLppeCp(const GnssConfigLppeControlPlaneMask lppeControlPlaneMask);
    static uint32_t convertLppeUp(const GnssConfigLppeUserPlaneMask lppeUserPlaneMask);
    static uint32_t convertAGloProt(const GnssConfigAGlonassPositionProtocolMask);
    static uint32_t convertSuplMode(const GnssConfigSuplModeMask suplModeMask);
    static void convertSatelliteInfo(std::vector<GnssDebugSatelliteInfo>& out,
                                     const GnssSvType& in_constellation,
                                     const SystemStatusReports& in);
    static bool convertToGnssSvIdConfig(
            const std::vector<GnssSvIdSource>& blacklistedSvIds, GnssSvIdConfig& config);
    static void convertFromGnssSvIdConfig(
            const GnssSvIdConfig& svConfig, std::vector<GnssSvIdSource>& blacklistedSvIds);
    static void convertGnssSvIdMaskToList(
            uint64_t svIdMask, std::vector<GnssSvIdSource>& svIds,
            GnssSvId initialSvId, GnssSvType svType);

    void injectLocationCommand(double latitude, double longitude, float accuracy);
    void injectLocationExtCommand(const GnssLocationInfoNotification &locationInfo);

    void injectTimeCommand(int64_t time, int64_t timeReference, int32_t uncertainty);
    void blockCPICommand(double latitude, double longitude, float accuracy,
                         int blockDurationMsec, double latLonDiffThreshold);

    void updatePowerStateCommand(PowerStateType powerState);

    /*==== DGnss Usable Report Flag ====================================================*/
    inline void setDGnssUsableFLag(bool dGnssNeedReport) { mDGnssNeedReport = dGnssNeedReport;}
};

#endif //GNSS_ADAPTER_H

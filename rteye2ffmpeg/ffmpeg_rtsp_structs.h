#pragma once


//REV: ghetto way to tell about private info of FFMPEG library...
//REV: note only works until they change those structures.
//REV: correct as of 5 oct 2022 -- ffmpeg 5.1.1, and probably works for 4.X.X
// etc.
// Any changes in ordering etc. of data struct must be reflected here.

// Note, sizes of these must be same too... i.e. pointer sizes, enum size, etc.

struct AVClass;
//struct URLContext;
struct URLProtocol;
struct RTSPStream;
struct MpegTSContext;
struct pollfd;


struct RTPDynamicProtocolHandler;
struct PayloadContext;


//REV: this size is actually maybe variable???? Depending on OS/arch...fuck
//assume it is in e.g. network.
/*struct sockaddr_storage {
    uint8_t ss_len;
    uint8_t ss_family;
    uint16_t ss_family;
    char ss_pad1[6];
    int64_t ss_align;
    char ss_pad2[112];
    };*/

#include <sys/socket.h>

/*typedef struct AVIOInterruptCB {
    int (*callback)(void*);
    void *opaque;
} AVIOInterruptCB;
*/

//internal define url.h
#define MAX_URL_SIZE 4096

typedef struct URLContext {
    const AVClass *av_class;    /**< information for av_log(). Set by url_open(). */
    const struct URLProtocol *prot;
    void *priv_data;
    char *filename;             /**< specified URL */
    int flags;
    int max_packet_size;        /**< if non zero, the stream is packetized with this max packet size */
    int is_streamed;            /**< true if streamed (no seek possible), default = false */
    int is_connected;
    AVIOInterruptCB interrupt_callback;
    int64_t rw_timeout;         /**< maximum time to wait for (network) read/write operation completion, in mcs */
    const char *protocol_whitelist;
    const char *protocol_blacklist;
    int min_packet_size;        /**< if non zero, the stream is packetized with this min packet size */
} URLContext;


typedef enum HTTPAuthType {
    HTTP_AUTH_NONE = 0,    /**< No authentication specified */
    HTTP_AUTH_BASIC,       /**< HTTP 1.0 Basic auth from RFC 1945
                             *  (also in RFC 2617) */
    HTTP_AUTH_DIGEST,      /**< HTTP 1.1 Digest auth from RFC 2617 */
} HTTPAuthType;

typedef struct DigestParams {
    char nonce[300];       /**< Server specified nonce */
    char algorithm[10];    /**< Server specified digest algorithm */
    char qop[30];          /**< Quality of protection, containing the one
                             *  that we've chosen to use, from the
                             *  alternatives that the server offered. */
    char opaque[300];      /**< A server-specified string that should be
                             *  included in authentication responses, not
                             *  included in the actual digest calculation. */
    char stale[10];        /**< The server indicated that the auth was ok,
                             * but needs to be redone with a new, non-stale
                             * nonce. */
    int nc;                /**< Nonce count, the number of earlier replies
                             *  where this particular nonce has been used. */
} DigestParams;

/**
 * HTTP Authentication state structure. Must be zero-initialized
 * before used with the functions below.
 */
typedef struct HTTPAuthState {
    /**
     * The currently chosen auth type.
     */
    int auth_type;
    /**
     * Authentication realm
     */
    char realm[200];
    /**
     * The parameters specific to digest authentication.
     */
    DigestParams digest_params;
    /**
     * Auth ok, but needs to be resent with a new nonce.
     */
    int stale;
} HTTPAuthState;

enum RTSPLowerTransport {
    RTSP_LOWER_TRANSPORT_UDP = 0,           /**< UDP/unicast */
    RTSP_LOWER_TRANSPORT_TCP = 1,           /**< TCP; interleaved in RTSP */
    RTSP_LOWER_TRANSPORT_UDP_MULTICAST = 2, /**< UDP/multicast */
    RTSP_LOWER_TRANSPORT_NB,
    RTSP_LOWER_TRANSPORT_HTTP = 8,          /**< HTTP tunneled - not a proper
                                                 transport mode as such,
                                                 only for use via AVOptions */
    RTSP_LOWER_TRANSPORT_HTTPS,             /**< HTTPS tunneled */
    RTSP_LOWER_TRANSPORT_CUSTOM = 16,       /**< Custom IO - not a public
                                                 option for lower_transport_mask,
                                                 but set in the SDP demuxer based
                                                 on a flag. */
};
enum RTSPTransport {
    RTSP_TRANSPORT_RTP, /**< Standards-compliant RTP */
    RTSP_TRANSPORT_RDT, /**< Realmedia Data Transport */
    RTSP_TRANSPORT_RAW, /**< Raw data (over UDP) */
    RTSP_TRANSPORT_NB
};
enum RTSPClientState {
    RTSP_STATE_IDLE,    /**< not initialized */
    RTSP_STATE_STREAMING, /**< initialized and sending/receiving data */
    RTSP_STATE_PAUSED,  /**< initialized, but not receiving data */
    RTSP_STATE_SEEKING, /**< initialized, requesting a seek */
};

enum RTSPServerType {
    RTSP_SERVER_RTP,  /**< Standards-compliant RTP-server */
    RTSP_SERVER_REAL, /**< Realmedia-style server */
    RTSP_SERVER_WMS,  /**< Windows Media server */
    RTSP_SERVER_SATIP,/**< SAT>IP server */
    RTSP_SERVER_NB
};
enum RTSPControlTransport {
    RTSP_MODE_PLAIN,   /**< Normal RTSP */
    RTSP_MODE_TUNNEL   /**< RTSP over HTTP (tunneling) */
};

typedef struct RTSPSource {
    char addr[128]; /**< Source-specific multicast include source IP address (from SDP content) */
} RTSPSource;




typedef struct RTSPStream {
    URLContext *rtp_handle;   /**< RTP stream handle (if UDP) */
    void *transport_priv; /**< RTP/RDT parse context if input, RTP AVFormatContext if output */

    /** corresponding stream index, if any. -1 if none (MPEG2TS case) */
    int stream_index;

    /** interleave IDs; copies of RTSPTransportField->interleaved_min/max
     * for the selected transport. Only used for TCP. */
    int interleaved_min, interleaved_max;

    char control_url[MAX_URL_SIZE];   /**< url for this stream (from SDP) */

    /** The following are used only in SDP, not RTSP */
    //@{
    int sdp_port;             /**< port (from SDP content) */
    struct sockaddr_storage sdp_ip; /**< IP address (from SDP content) */
    int nb_include_source_addrs; /**< Number of source-specific multicast include source IP addresses (from SDP content) */
    struct RTSPSource **include_source_addrs; /**< Source-specific multicast include source IP addresses (from SDP content) */
    int nb_exclude_source_addrs; /**< Number of source-specific multicast exclude source IP addresses (from SDP content) */
    struct RTSPSource **exclude_source_addrs; /**< Source-specific multicast exclude source IP addresses (from SDP content) */
    int sdp_ttl;              /**< IP Time-To-Live (from SDP content) */
    int sdp_payload_type;     /**< payload type */
    //@}

    /** The following are used for dynamic protocols (rtpdec_*.c/rdt.c) */
    //@{
    /** handler structure */
    const RTPDynamicProtocolHandler *dynamic_handler;

    /** private data associated with the dynamic protocol */
    PayloadContext *dynamic_protocol_context;
    //@}

    /** Enable sending RTCP feedback messages according to RFC 4585 */
    int feedback;

    /** SSRC for this stream, to allow identifying RTCP packets before the first RTP packet */
    uint32_t ssrc;

    char crypto_suite[40];
    char crypto_params[100];
} RTSPStream;



typedef struct RTSPState {
    const AVClass *_class;             /**< Class for private options. */
    URLContext *rtsp_hd; /* RTSP TCP connection handle */

    /** number of items in the 'rtsp_streams' variable */
    int nb_rtsp_streams;

    struct RTSPStream **rtsp_streams; /**< streams in this session */

    /** indicator of whether we are currently receiving data from the
     * server. Basically this isn't more than a simple cache of the
     * last PLAY/PAUSE command sent to the server, to make sure we don't
     * send 2x the same unexpectedly or commands in the wrong state. */
    enum RTSPClientState state;

    /** the seek value requested when calling av_seek_frame(). This value
     * is subsequently used as part of the "Range" parameter when emitting
     * the RTSP PLAY command. If we are currently playing, this command is
     * called instantly. If we are currently paused, this command is called
     * whenever we resume playback. Either way, the value is only used once,
     * see rtsp_read_play() and rtsp_read_seek(). */
    int64_t seek_timestamp;

    int seq;                          /**< RTSP command sequence number */

    /** copy of RTSPMessageHeader->session_id, i.e. the server-provided session
     * identifier that the client should re-transmit in each RTSP command */
    char session_id[512];

    /** copy of RTSPMessageHeader->timeout, i.e. the time (in seconds) that
     * the server will go without traffic on the RTSP/TCP line before it
     * closes the connection. */
    int timeout;

    /** timestamp of the last RTSP command that we sent to the RTSP server.
     * This is used to calculate when to send dummy commands to keep the
     * connection alive, in conjunction with timeout. */
    int64_t last_cmd_time;

    /** the negotiated data/packet transport protocol; e.g. RTP or RDT */
    enum RTSPTransport transport;

    /** the negotiated network layer transport protocol; e.g. TCP or UDP
     * uni-/multicast */
    enum RTSPLowerTransport lower_transport;

    /** brand of server that we're talking to; e.g. WMS, REAL or other.
     * Detected based on the value of RTSPMessageHeader->server or the presence
     * of RTSPMessageHeader->real_challenge */
    enum RTSPServerType server_type;

    /** the "RealChallenge1:" field from the server */
    char real_challenge[64];

    /** plaintext authorization line (username:password) */
    char auth[128];

    /** authentication state */
    HTTPAuthState auth_state;

    /** The last reply of the server to a RTSP command */
    char last_reply[2048]; /* XXX: allocate ? */

    /** RTSPStream->transport_priv of the last stream that we read a
     * packet from */
    void *cur_transport_priv;

    /** The following are used for Real stream selection */
    //@{
    /** whether we need to send a "SET_PARAMETER Subscribe:" command */
    int need_subscription;

    /** stream setup during the last frame read. This is used to detect if
     * we need to subscribe or unsubscribe to any new streams. */
    enum AVDiscard *real_setup_cache;

    /** current stream setup. This is a temporary buffer used to compare
     * current setup to previous frame setup. */
    enum AVDiscard *real_setup;

    /** the last value of the "SET_PARAMETER Subscribe:" RTSP command.
     * this is used to send the same "Unsubscribe:" if stream setup changed,
     * before sending a new "Subscribe:" command. */
    char last_subscription[1024];
    //@}

    /** The following are used for RTP/ASF streams */
    //@{
    /** ASF demuxer context for the embedded ASF stream from WMS servers */
    AVFormatContext *asf_ctx;

    /** cache for position of the asf demuxer, since we load a new
     * data packet in the bytecontext for each incoming RTSP packet. */
    uint64_t asf_pb_pos;
    //@}

    /** some MS RTSP streams contain a URL in the SDP that we need to use
     * for all subsequent RTSP requests, rather than the input URI; in
     * other cases, this is a copy of AVFormatContext->filename. */
    char control_uri[MAX_URL_SIZE];

    /** The following are used for parsing raw mpegts in udp */
    //@{
    struct MpegTSContext *ts;
    int recvbuf_pos;
    int recvbuf_len;
    //@}

    /** Additional output handle, used when input and output are done
     * separately, eg for HTTP tunneling. */
    URLContext *rtsp_hd_out;

    /** RTSP transport mode, such as plain or tunneled. */
    enum RTSPControlTransport control_transport;

    /* Number of RTCP BYE packets the RTSP session has received.
     * An EOF is propagated back if nb_byes == nb_streams.
     * This is reset after a seek. */
    int nb_byes;

    /** Reusable buffer for receiving packets */
    uint8_t* recvbuf;

    /**
     * A mask with all requested transport methods
     */
    int lower_transport_mask;

    /**
     * The number of returned packets
     */
    uint64_t packets;

    /**
     * Polling array for udp
     */
    struct pollfd *p;
    int max_p;

    /**
     * Whether the server supports the GET_PARAMETER method.
     */
    int get_parameter_supported;

    /**
     * Do not begin to play the stream immediately.
     */
    int initial_pause;

    /**
     * Option flags for the chained RTP muxer.
     */
    int rtp_muxer_flags;

    /** Whether the server accepts the x-Dynamic-Rate header */
    int accept_dynamic_rate;

    /**
     * Various option flags for the RTSP muxer/demuxer.
     */
    int rtsp_flags;

    /**
     * Mask of all requested media types
     */
    int media_type_mask;

    /**
     * Minimum and maximum local UDP ports.
     */
    int rtp_port_min, rtp_port_max;

    /**
     * Timeout to wait for incoming connections.
     */
    int initial_timeout;

    /**
     * timeout of socket i/o operations.
     */
    int64_t stimeout;

    /**
     * Size of RTP packet reordering queue.
     */
    int reordering_queue_size;

    /**
     * User-Agent string
     */
    char *user_agent;

    char default_lang[4];
    int buffer_size;
    int pkt_size;
    char *localaddr;
} RTSPState;


struct AVAES;
struct AVHMAC;
struct RTPDynamicProtocolHandler;
struct PayloadContext;

struct SRTPContext {
    struct AVAES *aes;
    struct AVHMAC *hmac;
    int rtp_hmac_size, rtcp_hmac_size;
    uint8_t master_key[16];
    uint8_t master_salt[14];
    uint8_t rtp_key[16],  rtcp_key[16];
    uint8_t rtp_salt[14], rtcp_salt[14];
    uint8_t rtp_auth[20], rtcp_auth[20];
    int seq_largest, seq_initialized;
    uint32_t roc;

    uint32_t rtcp_index;
};

// these statistics are used for rtcp receiver reports...
typedef struct RTPStatistics {
    uint16_t max_seq;           ///< highest sequence number seen
    uint32_t cycles;            ///< shifted count of sequence number cycles
    uint32_t base_seq;          ///< base sequence number
    uint32_t bad_seq;           ///< last bad sequence number + 1
    int probation;              ///< sequence packets till source is valid
    uint32_t received;          ///< packets received
    uint32_t expected_prior;    ///< packets expected in last interval
    uint32_t received_prior;    ///< packets received in last interval
    uint32_t transit;           ///< relative transit time for previous packet
    uint32_t jitter;            ///< estimated jitter.
} RTPStatistics;

typedef struct RTPPacket {
    uint16_t seq;
    uint8_t *buf;
    int len;
    int64_t recvtime;
    struct RTPPacket *next;
} RTPPacket;

struct RTPDemuxContext {
    AVFormatContext *ic;
    AVStream *st;
    int payload_type;
    uint32_t ssrc;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t base_timestamp;
    int64_t  unwrapped_timestamp;
    int64_t  range_start_offset;
    int max_payload_size;
    /* used to send back RTCP RR */
    char hostname[256];

    int srtp_enabled;
    struct SRTPContext srtp;

    /** Statistics for this stream (used by RTCP receiver reports) */
    RTPStatistics statistics;

    /** Fields for packet reordering @{ */
    int prev_ret;     ///< The return value of the actual parsing of the previous packet
    RTPPacket* queue; ///< A sorted queue of buffered packets not yet returned
    int queue_len;    ///< The number of packets in queue
    int queue_size;   ///< The size of queue, or 0 if reordering is disabled
    /*@}*/

    /* rtcp sender statistics receive */
    uint64_t last_rtcp_ntp_time;
    int64_t last_rtcp_reception_time;
    uint64_t first_rtcp_ntp_time;
    uint32_t last_rtcp_timestamp;
    int64_t rtcp_ts_offset;

    /* rtcp sender statistics */
    unsigned int packet_count;
    unsigned int octet_count;
    unsigned int last_octet_count;
    int64_t last_feedback_time;

    /* dynamic payload stuff */
    const RTPDynamicProtocolHandler *handler;
    PayloadContext *dynamic_protocol_context;
};

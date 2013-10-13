# Texr C SIP

TCSip is a library and daemon implementing VoIP softphone.

Features supported:

- voice calls
- encryption of voice trafic powered by SRTP
- encryption of signaling trafic powered by SSL
- X509 client certificates for both SIP and HTTPS
- history and contacts store available offline
- command line client
- operation as daemon controlled by IPC (local socket msgbuf)
- WebRTC-compatible sessions
- using ICE to pass voice trafic through NAT

# Operation systems

Tcsip proven to work on such platforms:

- iOS
- Mac OS X
- Android
- Linux

# Codecs

Supported codecs

- PCMU/PCMA
- Opus
- speex

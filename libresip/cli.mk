
lobj += rtp_io
lobj += rtp_opus
lobj += rtp_pcmu
lobj += rtp_speex

lobj += tcmedia
lobj += tcsipuser
lobj += tcsipreg
lobj += tcsipcall
lobj += tcuplinks
lobj += tcsip
lobj += tcipc
lobj += tcreport
lobj += x509util
lobj += x509gen
lobj += platpath

lobj-$(linux) += asound
lobj-$(apple) += sound
lobj-$(apple) += sound_utils
lobj-$(android) += opensl_io

lobj += ajitter

lobj += $(lobj-y)

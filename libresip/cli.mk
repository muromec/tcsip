
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
lobj += tcreport

lobj-$(linux) += asound
lobj-$(apple) += sound
lobj-$(apple) += sound_utils

lobj += ajitter

lobj += $(lobj-y)

## RFC 3366 Automatic Repeat Request (ARQ):
Implementation of RFC3366 using socket programming. Stop and wait ARQ and selective repeat ARQ were implemented over UDP protocol. A probabilistic function is used at client side to adjust the fault rate and show the working of the protocol. 
* Language : C 
* Operating System : Ubuntu 

### Patch 1:
This patch update the compatible string as per the new upstream convention from Rob H.
'''
Previously if was in the format <vendor>,<cc>-<target> like "qcom,videocc-sdm845"
Now we have updated it to <vendor>,<target_name>-<cc> like "qcom,sdm845-videocc".
'''

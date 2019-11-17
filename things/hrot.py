# Ham Radio of Things.
# WB2OSZ, November 2019

# You probably should not need to make any changes here.
# See other '*.py' files, in same directory, as examples of the part 
# you would customize for your own application.


import datetime
import time
import os
#import str
import re

#TODO: WLT, keep alive, ...
# APRS generally uses the destination field for a product id.

product_id = 'APMQ01'

# Receive and transmit Queue directories for communication with kissutil.
# Modern OS versions have a predefined RAM disk at /dev/shm.

rq_dir = '/dev/shm/RQ'
tq_dir = '/dev/shm/TQ'

# Send keep-alive message periodically so broker does not cancel subscription.

kpa_time_out = 30 * 60		# After 30 minutes from last message sent
#kpa_time_out = 30		#  temp test
kpa_count_down = kpa_time_out	# Count down each second.  


#----- aprs_msg -----

def aprs_msg(src,dst,via,addr,cmd,topic,value):
    """Create APRS 'message' from given components."""

    to = addr.ljust(9)[:9]
    msg = src + '>' + dst
    if via:
        msg += ',' + via
    msg += '::' + to + ':' + cmd + ':' + topic
    if len(value) > 0:
        msg += '=' + value
    return msg

#print (aprs_msg('WB2OSZ', product_id, '', 'N2GH', 'pub', 'button1', '1'))
#print (aprs_msg('WB2OSZ', product_id, 'WIDE1-1', 'WN2OSZ-15', 'sub', 'led', ''))
#print (aprs_msg('WB2OSZ', product_id, 'WIDE1-1,WIDE2-1', 'WN2OSZ-1', 'uns', '', ''))
#print (aprs_msg('WB2OSZ', product_id, 'WIDE1-1', 'WN2OSZ-15', 'pub', 'led', '(255,255,0)'))

#----- send_msg -----

def send_msg (chan, msg):
    """ Add message to transmit queue directory."""
    global kpa_count_down

    if not os.path.isdir(tq_dir):
        os.mkdir(tq_dir)
    if not os.path.isdir(rq_dir):
        os.mkdir(rq_dir)

    t = datetime.datetime.now()
    #fname = tq_dir + '/' + t.strftime("%y.%j.%H%M%S.%f")[:17]
    fname = tq_dir + '/' + t.strftime("%y%m%d.%H%M%S.%f")[:17]

    try:
        f = open(fname, 'w')
    except:
        print ("Failed to open " + fname + " for write")
    else:
        if chan > 0:
            f.write('[' + str(chan) + '] ' + msg + '\n')
        else:
            f.write(msg + '\n')
        f.close()
        kpa_count_down = kpa_time_out
    time.sleep (0.002)	# Ensure unique names


#send_msg(5, "abcd")
#send_msg(0, "wxyz")



#----- publish -----

def publish(chan,mycall,via,broker,topic,value):
    """Send message to publish value for a topic."""

    m = aprs_msg(mycall, product_id, via, broker, 'pub', topic, value)
    send_msg (chan, m)



#----- subscribe -----

def subscribe(chan,mycall,via,broker,topic):
    """Send message to subscribe to a topic, possibly wildcarded."""

    m = aprs_msg(mycall, product_id, via, broker, 'sub', topic, '')
    send_msg (chan, m)




#----- keep_alive -----

def keep_alive(chan,mycall,via,broker):
    """Tell broker:  I'm not dead.  I'm getting better.  I don't want to go on the cart."""

    m = aprs_msg(mycall, product_id, via, broker, 'kpa', '', '')
    send_msg (chan, m)



#----- parse_msg -----

def parse_msg (chan, msg, callback):
    """Parse an APRS 'message'.  The first character is already known to be ':'."""
    if msg[0] != ':' or msg[10] != ':':
        print ('Not message format - ' + msg)
        return
    addressee = msg[1:10].rstrip()
    # In the future we will probably have more options between the 3 letter command
    # and the next ':' but for now consider only 3 letters.
    # It is case-insensitive.  Convert to lower case before compare.
    topic = ''
    value = ''
    m = re.search (r'^([a-zA-Z]{3})([^:]*):(([^=]+)(=(.*))?)?$', msg[11:])
    if m:
        cmd = m.group(1).lower()
        options = m.group(2)
        if m.group(4):
            topic = m.group(4)
        if m.group(6):
            value = m.group(6)

        #print ('#'+addressee+'#'+cmd+'#'+options+'#'+topic+'#'+value+'#')
        callback (addressee,cmd,options,topic,value)



#----- parse_aprs -----

def parse_aprs (packet, callback):
    """Parse and APRS packet, possibly prefixed by channel number."""

    print (packet)
    if len(packet) == 0:
        return

    chan = ''
    # Split into address and information parts.
    # There could be a leading '[n]' with a channel number.
    m = re.search (r'^(\[.+\] *)?([^:]+):(.+)$', packet)
    if m:
        chan = m.group(1)	# Still enclosed in [].
        addrs = m.group(2)
        info = m.group(3)
        #print ('<>'+addrs+'<>'+info+'<>')

        if info[0] == '}':
            # Unwrap third party traffic format
            # Preserve any channel.
            if chan:
                parse_aprs (chan + info[1:], callback)
            else:
                parse_aprs (info[1:], callback)
        elif info[0] == ':':
            # APRS "message" format.  Is it HROT format?
            #print ('Process "message" - ' + info)
            parse_msg (chan, info, callback)
        else:
            print ('Not APRS "message" format - ' + info)
    else:
        print ('Could not split into address & info parts - ' + packet)


#----- recv_loop -----

def recv_loop(chan,mycall,via,broker,callback):
    """Poll the receive queue directory and invoke 'callback' function when something found."""
    global kpa_count_down

    if not os.path.isdir(tq_dir):
        os.mkdir(tq_dir)
    if not os.path.isdir(rq_dir):
        os.mkdir(rq_dir)

    while True:
        time.sleep(1)
        #print ('polling')
        try:
            files = os.listdir(rq_dir)
        except:
            print ('Could not get listing of directory ' + rq_dir + '\n')
            quit()

        files.sort()
        for f in files:
            fname = rq_dir + '/' + f
            print (fname)
            if os.path.isfile(fname):
                print ('---')
                print ('Processing ' + fname + ' ...')
                with open (fname, 'r') as h:
                    for m in h:
                        m.rstrip('\n')
                        parse_aprs (m.rstrip('\n'), callback)
                os.remove(fname)
            else:
 		#print (fname + ' is not an ordinary file - ignore')
                pass

        kpa_count_down = kpa_count_down - 1
        if kpa_count_down <= 0:
            keep_alive (chan,mycall,via,broker)
            # Message being sent should reset kpa_count_down.



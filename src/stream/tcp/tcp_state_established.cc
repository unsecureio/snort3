//--------------------------------------------------------------------------
// Copyright (C) 2015-2016 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

// tcp_state_established.cc author davis mcpherson <davmcphe@@cisco.com>
// Created on: Jul 30, 2015

#include "tcp_module.h"
#include "tcp_tracker.h"
#include "tcp_session.h"
#include "tcp_normalizer.h"
#include "tcp_state_established.h"

TcpStateEstablished::TcpStateEstablished(TcpStateMachine& tsm, TcpSession& ssn) :
    TcpStateHandler(TcpStreamTracker::TCP_ESTABLISHED, tsm, ssn)
{
}

TcpStateEstablished::~TcpStateEstablished()
{
}

bool TcpStateEstablished::syn_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    session.check_for_repeated_syn(tsd);

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::syn_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    session.check_for_repeated_syn(tsd);

    trk.normalizer->ecn_tracker(tsd.get_tcph(), session.config->require_3whs() );

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::syn_ack_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    if ( session.config->midstream_allowed(tsd.get_pkt()) )
    {
        session.update_session_on_syn_ack( );
    }

    if ( trk.is_server_tracker() )
        trk.normalizer->ecn_tracker(tsd.get_tcph(), session.config->require_3whs() );

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::syn_ack_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::ack_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    trk.update_tracker_ack_sent(tsd);

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::ack_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    trk.update_tracker_ack_recv(tsd);

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::data_seg_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    trk.update_tracker_ack_sent(tsd);

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::data_seg_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    trk.update_tracker_ack_recv(tsd);
    session.handle_data_segment(tsd);

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::fin_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    trk.update_on_fin_sent(tsd);
    session.eof_handle(tsd.get_pkt());
    trk.set_tcp_state(TcpStreamTracker::TCP_FIN_WAIT1);

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::fin_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    trk.update_tracker_ack_recv(tsd);
    if ( tsd.get_seg_len() > 0 )
    {
        session.handle_data_segment(tsd);
        trk.flush_data_on_fin_recv(tsd);
    }
    trk.update_on_fin_recv(tsd);
    session.update_perf_base_state(TcpStreamTracker::TCP_CLOSING);
    trk.set_tcp_state(TcpStreamTracker::TCP_CLOSE_WAIT);

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::rst_sent(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::rst_recv(TcpSegmentDescriptor& tsd, TcpStreamTracker& tracker)
{
    auto& trk = static_cast< TcpStreamTracker& >( tracker );

    if ( trk.update_on_rst_recv(tsd) )
    {
        session.update_session_on_rst(tsd, false);
        session.update_perf_base_state(TcpStreamTracker::TCP_CLOSING);
        session.set_pkt_action_flag(ACTION_RST);
    }
    else
    {
        session.tel.set_tcp_event(EVENT_BAD_RST);
    }

    // FIXIT - might be good to create alert specific to RST with data
    if ( tsd.get_seg_len() > 0 )
        session.tel.set_tcp_event(EVENT_DATA_AFTER_RST_RCVD);

    return default_state_action(tsd, trk);
}

bool TcpStateEstablished::do_pre_sm_packet_actions(TcpSegmentDescriptor& tsd)
{
    return session.validate_packet_established_session(tsd);
}

bool TcpStateEstablished::do_post_sm_packet_actions(TcpSegmentDescriptor& tsd)
{
    session.update_paws_timestamps(tsd);

    session.check_for_window_slam(tsd);

    return true;
}


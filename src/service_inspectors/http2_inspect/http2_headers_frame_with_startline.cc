//--------------------------------------------------------------------------
// Copyright (C) 2020-2020 Cisco and/or its affiliates. All rights reserved.
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
// http2_headers_frame_with_startline.cc author Katura Harvey <katharve@cisco.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "http2_headers_frame_with_startline.h"

#include "protocols/packet.h"
#include "service_inspectors/http_inspect/http_enum.h"
#include "service_inspectors/http_inspect/http_flow_data.h"
#include "service_inspectors/http_inspect/http_inspect.h"
#include "service_inspectors/http_inspect/http_stream_splitter.h"

#include "http2_dummy_packet.h"
#include "http2_enum.h"
#include "http2_flow_data.h"
#include "http2_hpack.h"
#include "http2_request_line.h"
#include "http2_start_line.h"
#include "http2_status_line.h"
#include "http2_stream.h"

using namespace snort;
using namespace HttpCommon;
using namespace Http2Enums;


Http2HeadersFrameWithStartline::~Http2HeadersFrameWithStartline()
{
    delete start_line_generator;
}

bool Http2HeadersFrameWithStartline::process_start_line(HttpFlowData*& http_flow,
    SourceId hi_source_id)
{
    if (session_data->abort_flow[source_id])
        return false;

    // http_inspect scan() of start line
    {
        uint32_t flush_offset;
        Http2DummyPacket dummy_pkt;
        dummy_pkt.flow = session_data->flow;
        const uint32_t unused = 0;
        const StreamSplitter::Status start_scan_result =
            session_data->hi_ss[hi_source_id]->scan(&dummy_pkt, start_line.start(),
                start_line.length(), unused, &flush_offset);
        assert(start_scan_result == StreamSplitter::FLUSH);
        UNUSED(start_scan_result);
        assert((int64_t)flush_offset == start_line.length());
    }

    StreamBuffer stream_buf;

    // http_inspect reassemble() of start line
    {
        unsigned copied;
        stream_buf = session_data->hi_ss[hi_source_id]->reassemble(session_data->flow,
            start_line.length(), 0, start_line.start(), start_line.length(), PKT_PDU_TAIL,
            copied);
        assert(stream_buf.data != nullptr);
        assert(copied == (unsigned)start_line.length());
    }

    http_flow = stream->get_hi_flow_data();
    assert(http_flow);
    // http_inspect eval() and clear() of start line
    {
        Http2DummyPacket dummy_pkt;
        dummy_pkt.flow = session_data->flow;
        dummy_pkt.packet_flags = (hi_source_id == SRC_CLIENT) ? PKT_FROM_CLIENT : PKT_FROM_SERVER;
        dummy_pkt.dsize = stream_buf.length;
        dummy_pkt.data = stream_buf.data;
        session_data->hi->eval(&dummy_pkt);
        if (http_flow->get_type_expected(hi_source_id) != HttpEnums::SEC_HEADER)
        {
            *session_data->infractions[source_id] += INF_INVALID_STARTLINE;
            session_data->events[source_id]->create_event(EVENT_INVALID_STARTLINE);
            stream->set_state(hi_source_id, STREAM_ERROR);
            return false;
        }
        session_data->hi->clear(&dummy_pkt);
    }
    return true;
}


#ifdef REG_TEST
void Http2HeadersFrameWithStartline::print_frame(FILE* output)
{
    start_line.print(output, "Decoded start-line");
    Http2HeadersFrame::print_frame(output);
}
#endif
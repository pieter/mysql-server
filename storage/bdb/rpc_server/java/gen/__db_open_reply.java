/*
 * Automatically generated by jrpcgen 0.95.1 on 7/15/04 4:39 PM
 * jrpcgen is part of the "Remote Tea" ONC/RPC package for Java
 * See http://acplt.org/ks/remotetea.html for details
 */
package com.sleepycat.db.rpcserver;
import org.acplt.oncrpc.*;
import java.io.IOException;

public class __db_open_reply implements XdrAble {
    public int status;
    public int dbcl_id;
    public int type;
    public int lorder;

    public __db_open_reply() {
    }

    public __db_open_reply(XdrDecodingStream xdr)
           throws OncRpcException, IOException {
        xdrDecode(xdr);
    }

    public void xdrEncode(XdrEncodingStream xdr)
           throws OncRpcException, IOException {
        xdr.xdrEncodeInt(status);
        xdr.xdrEncodeInt(dbcl_id);
        xdr.xdrEncodeInt(type);
        xdr.xdrEncodeInt(lorder);
    }

    public void xdrDecode(XdrDecodingStream xdr)
           throws OncRpcException, IOException {
        status = xdr.xdrDecodeInt();
        dbcl_id = xdr.xdrDecodeInt();
        type = xdr.xdrDecodeInt();
        lorder = xdr.xdrDecodeInt();
    }

}
// End of __db_open_reply.java
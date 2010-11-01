/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
/*
This file contains the implementation for the classes for the DT Actions,
Data Transfer Related Actions (Table 9-8 of ps 3.8-2009).

Since each class is essentially a placeholder for a function pointer, I'm breaking with having
each class have its own file for the sake of brevity of the number of files.
*/

#include "gdcmULActionDT.h"
#include "gdcmARTIMTimer.h"
#include "gdcmPDataTFPDU.h"

#include "gdcmPDataTFPDU.h"
#include "gdcmAttribute.h"
#include "gdcmFile.h"
#include "gdcmDataSet.h"
#include "gdcmAReleaseRPPDU.h"
#include "gdcmAAssociateRQPDU.h"
#include "gdcmAAssociateACPDU.h"
#include "gdcmAReleaseRQPDU.h"

#include <socket++/echo.h>//for setting up the local socket

using namespace gdcm::network;

static void process_input(iosockinet& sio)
{
  uint8_t itemtype = 0x0;
  sio.read( (char*)&itemtype, 1 );
  assert( itemtype == 0x1 );

  gdcm::network::AAssociateRQPDU rqpdu;
  //rqpdu.SetCallingAETitle( "MOTESCU" );
  rqpdu.Read( sio );
  rqpdu.Print( std::cout );

  //std::cout << "done AAssociateRQPDU !" << std::endl;

  gdcm::network::TransferSyntaxSub ts1;
  ts1.SetNameFromUID( gdcm::UIDs::ImplicitVRLittleEndianDefaultTransferSyntaxforDICOM );

  gdcm::network::AAssociateACPDU acpdu;

  for( unsigned int index = 0; index < rqpdu.GetNumberOfPresentationContext(); index++ )
    {
    // FIXME / HARDCODED We only ever accept Little Endian
    // FIXME we should check :
    // rqpdu.GetAbstractSyntax() contains LittleENdian
    gdcm::network::PresentationContextAC pcac1;
    PresentationContext const &pc = rqpdu.GetPresentationContext(index);
    uint8_t id = pc.GetPresentationContextID();

    pcac1.SetPresentationContextID( id ); // DCMTK MR
    pcac1.SetTransferSyntax( ts1 );
    acpdu.AddPresentationContextAC( pcac1 );
    }

  acpdu.Write( sio );
  sio.flush();

  //std::cout << "done AAssociateACPDU !" << std::endl;

  sio.read( (char*)&itemtype, 1 );
  assert( itemtype == 0x4 );

  gdcm::network::PDataTFPDU pdata;
  pdata.Read( sio );
  pdata.Print( std::cout );
  // pick the first one:
  size_t n = pdata.GetNumPDVs();

  assert( n == 1 );
  gdcm::network::PresentationDataValue const &input_pdv = pdata.GetPresentationDataValue(0);

  //std::cout << "done PDataTFPDU 1!" << std::endl;

  uint8_t messageheader;
  messageheader = input_pdv.GetMessageHeader();

  //std::cout << "Start with MessageHeader : " << (int)messageheader << std::endl;

  gdcm::Attribute<0x0,0x800> at = { 0 };
  //at.SetFromDataSet( input_pdv.GetDataSet() );
  unsigned short commanddatasettype = at.GetValue();
  //std::cout << "CommandDataSetType: " << at.GetValue() << std::endl;
  assert( messageheader == 3 );

  // C-STORE
  if( commanddatasettype == 0 )
    {
    std::ofstream out( "movescu.dcm" );
    int i = 0;
    do
      {
      gdcm::network::PDataTFPDU pdata2;
      pdata2.ReadInto( sio, out );
      //pdata2.Print( std::cout );
      size_t n2 = pdata.GetNumPDVs();
      assert( n2 == 1 );
      gdcm::network::PresentationDataValue const &pdv = pdata2.GetPresentationDataValue(0);
      messageheader = pdv.GetMessageHeader();
      //std::cout << "---------------- done PDataTFPDU: " << i << std::endl;
      //std::cout << "---------------- done MessageHeader: " << (int)messageheader << std::endl;
      ++i;
      }
    while( messageheader == 0 );
    assert( messageheader == 2 ); // end of data
    out.close();

    gdcm::network::PresentationDataValue pdv;
    pdv.SetPresentationContextID( input_pdv.GetPresentationContextID() );
    std::vector<PresentationDataValue> inpdvs;
    inpdvs.push_back( input_pdv );
    gdcm::DataSet ds1 = PresentationDataValue::ConcatenatePDVBlobs( inpdvs );

    const gdcm::DataElement &de1 = ds1.GetDataElement( gdcm::Tag( 0x0000,0x0002 ) );
    const gdcm::ByteValue *bv1 = de1.GetByteValue();
    std::string s1( bv1->GetPointer(), bv1->GetLength() );
    const gdcm::DataElement &de2 = ds1.GetDataElement( gdcm::Tag( 0x0000,0x1000 ) );
    const gdcm::ByteValue *bv2 = de2.GetByteValue();
    std::string s2( bv2->GetPointer(), bv2->GetLength() );

    //pdv.MyInit2( s1.c_str(), s2.c_str() );

    //std::cout << "Compare:" << std::endl;
    //input_pdv.Print( std::cout );
    //std::cout << "To:" << std::endl;
    //pdv.Print( std::cout );

    gdcm::network::PDataTFPDU pdata4;
    pdata4.AddPresentationDataValue( pdv );
    pdata4.Write( sio );
    //sio.flush();
    }

  //sio.read( (char*)&itemtype, 1 );
  //assert( itemtype == 0x4 );
  //gdcm::network::AReleaseRQPDU rel0;
  //rel0.Read( sio );

  // send release:
  gdcm::network::AReleaseRPPDU rel;
  rel.Write( sio );
  sio.flush();

  //std::cout << "done AReleaseRPPDU!" << std::endl;

  gdcm::network::AReleaseRPPDU rel2;
  //rel2.Write( sio );
  //sio.flush();
}

//Send P-DATA-TF PDU
EStateID ULActionDT1::PerformAction(ULEvent& inEvent, ULConnection& inConnection,
        bool& outWaitingForEvent, EEventID& outRaisedEvent)
{
  std::vector<BasePDU*> theDataPDUs = inEvent.GetPDUs();
  std::vector<BasePDU*>::const_iterator itor = theDataPDUs.begin();
  //they can all be sent at once because of the structure in 3.8 7.6-- pdata does not wait for a response.
  for (itor = theDataPDUs.begin(); itor < theDataPDUs.end(); itor++) {

    PDataTFPDU* dataPDU = dynamic_cast<PDataTFPDU*>(*itor);
    if (dataPDU == NULL){
      throw new gdcm::Exception("Data sending event PDU malformed.");
      return eStaDoesNotExist;
    }
    dataPDU->Write(*inConnection.GetProtocol());
    inConnection.GetProtocol()->flush();

  }

  // When doing a C-MOVE we recevie the Requested DataSet over
  // another chanel (technically this is send to an SCP)
  // in our case we use another port to receive it.

#if 0
  //wait for the user to try to send some data.
  sockinetbuf sin (sockbuf::sock_stream);

  sin.bind( 5677 );

  //std::cout << "localhost = " << sin.localhost() << std::endl
  //  << "localport = " << sin.localport() << std::endl;

  sin.listen();

//  for(;;)
    {
    iosockinet s (sin.accept());
    process_input(s);
    }
#endif


  outWaitingForEvent = true;//wait for a response that the data got there.
  outRaisedEvent = ePDATArequest;

  return eSta6TransferReady;
}

//Send P-DATA indication primitive
//for now, does nothing, stops the event loop
EStateID ULActionDT2::PerformAction(ULEvent& inEvent, ULConnection& inConnection,
        bool& outWaitingForEvent, EEventID& outRaisedEvent){

  outWaitingForEvent = false;
  outRaisedEvent = ePDATArequest;
  return eSta6TransferReady;
}

/** @file
  SPDM common library.
  It follows the SPDM Specification.

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SpdmRequesterLibInternal.h"

#pragma pack(1)

typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT16               PortionLength;
  UINT16               RemainderLength;
  UINT8                CertChain[MAX_SPDM_CERT_CHAIN_BLOCK_LEN];
} SPDM_CERTIFICATE_RESPONSE_MAX;

#pragma pack()

/**
  This function verifies the integrity of certificate chain.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  CertificateChain             The certificate chain data buffer.
  @param  CertificateChainSize         Size in bytes of the certificate chain data buffer.

  @retval TRUE  certificate chain integrity verification pass.
  @retval FALSE certificate chain integrity verification fail.
**/
BOOLEAN
SpdmRequesterVerifyCertificateChainData (
  IN SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN VOID                         *CertificateChain,
  UINTN                           CertificateChainSize
  )
{
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  UINT8                                     *RootCertBuffer;
  UINTN                                     RootCertBufferSize;
  UINTN                                     HashSize;
  UINT8                                     CalcRootCertHash[MAX_HASH_SIZE];
  UINT8                                     *LeafCertBuffer;
  UINTN                                     LeafCertBufferSize;

  HashSize = GetSpdmHashSize (SpdmContext);

  if (CertificateChainSize > MAX_SPDM_MESSAGE_BUFFER_SIZE) {
    DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (buffer too large) !!!\n"));
    return FALSE;
  }

  if (CertificateChainSize <= sizeof(SPDM_CERT_CHAIN) + HashSize) {
    DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (buffer too small) !!!\n"));
    return FALSE;
  }

  CertBuffer = (UINT8 *)CertificateChain + sizeof(SPDM_CERT_CHAIN) + HashSize;
  CertBufferSize = CertificateChainSize - sizeof(SPDM_CERT_CHAIN) - HashSize;
  if (!X509GetCertFromCertChain (CertBuffer, CertBufferSize, 0, &RootCertBuffer, &RootCertBufferSize)) {
    DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (get root certificate failed)!!!\n"));
    return FALSE;
  }

  SpdmHashAll (SpdmContext, RootCertBuffer, RootCertBufferSize, CalcRootCertHash);
  if (CompareMem ((UINT8 *)CertificateChain + sizeof(SPDM_CERT_CHAIN), CalcRootCertHash, HashSize) != 0) {
    DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (cert root hash mismatch) !!!\n"));
    return FALSE;
  }

  if (!X509VerifyCertChain (RootCertBuffer, RootCertBufferSize, CertBuffer, CertBufferSize)) {
    DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (cert chain verify failed)!!!\n"));
    return FALSE;
  }

  if (!X509GetCertFromCertChain (CertBuffer, CertBufferSize, -1, &LeafCertBuffer, &LeafCertBufferSize)) {
    DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (get leaf certificate failed)!!!\n"));
    return FALSE;
  }

  if(!SpdmX509CertificateCheck (LeafCertBuffer, LeafCertBufferSize)) {
    DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (leaf certificate check failed)!!!\n"));
    return FALSE;
  }

  return TRUE;
}

/**
  This function verifies the certificate chain.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  CertificateChain             The certificate chain data buffer.
  @param  CertificateChainSize         Size in bytes of the certificate chain data buffer.

  @retval TRUE  certificate chain verification pass.
  @retval FALSE certificate chain verification fail.
**/
BOOLEAN
SpdmRequesterVerifyCertificateChain (
  IN SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN VOID                         *CertificateChain,
  UINTN                           CertificateChainSize
  )
{
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  UINTN                                     HashSize;
  UINT8                                     *RootCertHash;
  UINTN                                     RootCertHashSize;
  BOOLEAN                                   Result;

  Result = SpdmRequesterVerifyCertificateChainData(SpdmContext, CertificateChain, CertificateChainSize);
  if (!Result) {
    return FALSE;
  }

  RootCertHash = SpdmContext->LocalContext.PeerRootCertHashProvision;
  RootCertHashSize = SpdmContext->LocalContext.PeerRootCertHashProvisionSize;
  CertBuffer = SpdmContext->LocalContext.PeerCertChainProvision;
  CertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize;

  if ((RootCertHash != NULL) && (RootCertHashSize != 0)) {
    HashSize = GetSpdmHashSize (SpdmContext);
    if (RootCertHashSize != HashSize) {
      DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (hash size mismatch) !!!\n"));
      return FALSE;
    }
    if (CompareMem ((UINT8 *)CertificateChain + sizeof(SPDM_CERT_CHAIN), RootCertHash, HashSize) != 0) {
      DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (root hash mismatch) !!!\n"));
      return FALSE;
    }
  } else if ((CertBuffer != NULL) && (CertBufferSize != 0)) {
    if (CertBufferSize != CertificateChainSize) {
      DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL !!!\n"));
      return FALSE;
    }
    if (CompareMem (CertificateChain, CertBuffer, CertificateChainSize) != 0) {
      DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL !!!\n"));
      return FALSE;
    }
  }

  DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - PASS !!!\n"));
  SpdmContext->ConnectionInfo.PeerCertChainBufferSize = CertificateChainSize;
  CopyMem (SpdmContext->ConnectionInfo.PeerCertChainBuffer, CertificateChain, CertificateChainSize);

  return TRUE;
}

/**
  This function sends GET_CERTIFICATE
  to get certificate chain in one slot from device.

  This function verify the integrity of the certificate chain.
  RootHash -> Root certificate -> Intermediate certificate -> Leaf certificate.

  If the peer root certificate hash is deployed,
  this function also verifies the digest with the root hash in the certificate chain.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SlotNum                      The number of slot for the certificate chain.
  @param  Length                       Length parameter in the get_certificate message (limited by MAX_SPDM_CERT_CHAIN_BLOCK_LEN).
  @param  CertChainSize                On input, indicate the size in bytes of the destination buffer to store the digest buffer.
                                       On output, indicate the size in bytes of the certificate chain.
  @param  CertChain                    A pointer to a destination buffer to store the certificate chain.

  @retval RETURN_SUCCESS               The certificate chain is got successfully.
  @retval RETURN_DEVICE_ERROR          A device error occurs when communicates with the device.
  @retval RETURN_SECURITY_VIOLATION    Any verification fails.
**/
RETURN_STATUS
TrySpdmGetCertificate (
  IN     VOID                 *Context,
  IN     UINT8                SlotNum,
  IN     UINT16               Length,
  IN OUT UINTN                *CertChainSize,
     OUT VOID                 *CertChain
  )
{
  BOOLEAN                                   Result;
  RETURN_STATUS                             Status;
  SPDM_GET_CERTIFICATE_REQUEST              SpdmRequest;
  SPDM_CERTIFICATE_RESPONSE_MAX             SpdmResponse;
  UINTN                                     SpdmResponseSize;
  LARGE_MANAGED_BUFFER                      CertificateChainBuffer;
  SPDM_DEVICE_CONTEXT                       *SpdmContext;

  InitManagedBuffer (&CertificateChainBuffer, MAX_SPDM_MESSAGE_BUFFER_SIZE);
  Length = MIN(Length, MAX_SPDM_CERT_CHAIN_BLOCK_LEN);

  SpdmContext = Context;
  if (((SpdmContext->SpdmCmdReceiveState & SPDM_GET_DIGESTS_RECEIVE_FLAG) == 0) ||
      ((SpdmContext->SpdmCmdReceiveState & SPDM_GET_CAPABILITIES_RECEIVE_FLAG) == 0)) {
    return RETURN_DEVICE_ERROR;
  }
  if ((SpdmContext->ConnectionInfo.Capability.Flags & SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CERT_CAP) == 0) {
    return RETURN_DEVICE_ERROR;
  }

  if (SlotNum >= MAX_SPDM_SLOT_COUNT) {
    return RETURN_INVALID_PARAMETER;
  }

  SpdmContext->ErrorState = SPDM_STATUS_ERROR_DEVICE_NO_CAPABILITIES;

  do {
    if (SpdmIsVersionSupported (SpdmContext, SPDM_MESSAGE_VERSION_11)) {
      SpdmRequest.Header.SPDMVersion = SPDM_MESSAGE_VERSION_11;
    } else {
      SpdmRequest.Header.SPDMVersion = SPDM_MESSAGE_VERSION_10;
    }
    SpdmRequest.Header.RequestResponseCode = SPDM_GET_CERTIFICATE;
    SpdmRequest.Header.Param1 = SlotNum;
    SpdmRequest.Header.Param2 = 0;
    SpdmRequest.Offset = (UINT16)GetManagedBufferSize (&CertificateChainBuffer);
    SpdmRequest.Length = Length;
    DEBUG((DEBUG_INFO, "Request (Offset 0x%x, Size 0x%x):\n", SpdmRequest.Offset, SpdmRequest.Length));

    Status = SpdmSendSpdmRequest (SpdmContext, NULL, sizeof(SpdmRequest), &SpdmRequest);
    if (RETURN_ERROR(Status)) {
      Status = RETURN_DEVICE_ERROR;
      goto Done;
    }

    //
    // Cache data
    //
    AppendManagedBuffer (&SpdmContext->Transcript.MessageB, &SpdmRequest, sizeof(SpdmRequest));

    SpdmResponseSize = sizeof(SpdmResponse);
    ZeroMem (&SpdmResponse, sizeof(SpdmResponse));
    Status = SpdmReceiveSpdmResponse (SpdmContext, NULL, &SpdmResponseSize, &SpdmResponse);
    if (RETURN_ERROR(Status)) {
      Status = RETURN_DEVICE_ERROR;
      goto Done;
    }
    if (SpdmResponseSize < sizeof(SPDM_MESSAGE_HEADER)) {
      Status = RETURN_DEVICE_ERROR;
      goto Done;
    }
    if (SpdmResponse.Header.RequestResponseCode == SPDM_ERROR) {
      Status = SpdmHandleErrorResponseMain(SpdmContext, &SpdmContext->Transcript.MessageB, sizeof(SpdmRequest), &SpdmResponseSize, &SpdmResponse, SPDM_GET_CERTIFICATE, SPDM_CERTIFICATE, sizeof(SPDM_CERTIFICATE_RESPONSE_MAX));
      if (RETURN_ERROR(Status)) {
        goto Done;
      }
    } else if (SpdmResponse.Header.RequestResponseCode != SPDM_CERTIFICATE) {
      Status = RETURN_DEVICE_ERROR;
      goto Done;
    }
    if (SpdmResponseSize < sizeof(SPDM_CERTIFICATE_RESPONSE)) {
      Status = RETURN_DEVICE_ERROR;
      goto Done;
    }
    if (SpdmResponseSize > sizeof(SpdmResponse)) {
      Status = RETURN_DEVICE_ERROR;
      goto Done;
    }
    if (SpdmResponse.PortionLength > MAX_SPDM_CERT_CHAIN_BLOCK_LEN) {
      Status = RETURN_DEVICE_ERROR;
      goto Done;
    }
    if (SpdmResponse.Header.Param1 != SlotNum) {
      Status = RETURN_DEVICE_ERROR;
      goto Done;
    }
    if (SpdmResponseSize < sizeof(SPDM_CERTIFICATE_RESPONSE) + SpdmResponse.PortionLength) {
      Status = RETURN_DEVICE_ERROR;
      goto Done;
    }
    SpdmResponseSize = sizeof(SPDM_CERTIFICATE_RESPONSE) + SpdmResponse.PortionLength;
    //
    // Cache data
    //
    AppendManagedBuffer (&SpdmContext->Transcript.MessageB, &SpdmResponse, SpdmResponseSize);

    DEBUG((DEBUG_INFO, "Certificate (Offset 0x%x, Size 0x%x):\n", SpdmRequest.Offset, SpdmResponse.PortionLength));
    InternalDumpHex (SpdmResponse.CertChain, SpdmResponse.PortionLength);

    AppendManagedBuffer (&CertificateChainBuffer, SpdmResponse.CertChain, SpdmResponse.PortionLength);

  } while (SpdmResponse.RemainderLength != 0);

  Result = SpdmRequesterVerifyCertificateChain (SpdmContext, GetManagedBuffer(&CertificateChainBuffer), GetManagedBufferSize(&CertificateChainBuffer));
  if (!Result) {
    SpdmContext->ErrorState = SPDM_STATUS_ERROR_CERTIFICATE_FAILURE;
    Status = RETURN_SECURITY_VIOLATION;
    goto Done;
  }

  SpdmContext->ErrorState = SPDM_STATUS_SUCCESS;

  if (CertChainSize != NULL) {
    if (*CertChainSize < GetManagedBufferSize(&CertificateChainBuffer)) {
      *CertChainSize = GetManagedBufferSize(&CertificateChainBuffer);
      return RETURN_BUFFER_TOO_SMALL;
    }
    *CertChainSize = GetManagedBufferSize(&CertificateChainBuffer);
    if (CertChain != NULL) {
      CopyMem (
        CertChain,
        GetManagedBuffer(&CertificateChainBuffer),
        GetManagedBufferSize(&CertificateChainBuffer)
        );
    }
  }
  SpdmContext->SpdmCmdReceiveState |= SPDM_GET_CERTIFICATE_RECEIVE_FLAG;
  Status = RETURN_SUCCESS;
Done:
  return Status;
}

/**
  This function sends GET_CERTIFICATE
  to get certificate chain in one slot from device.

  This function verify the integrity of the certificate chain.
  RootHash -> Root certificate -> Intermediate certificate -> Leaf certificate.

  If the peer root certificate hash is deployed,
  this function also verifies the digest with the root hash in the certificate chain.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SlotNum                      The number of slot for the certificate chain.
  @param  CertChainSize                On input, indicate the size in bytes of the destination buffer to store the digest buffer.
                                       On output, indicate the size in bytes of the certificate chain.
  @param  CertChain                    A pointer to a destination buffer to store the certificate chain.

  @retval RETURN_SUCCESS               The certificate chain is got successfully.
  @retval RETURN_DEVICE_ERROR          A device error occurs when communicates with the device.
  @retval RETURN_SECURITY_VIOLATION    Any verification fails.
**/
RETURN_STATUS
EFIAPI
SpdmGetCertificate (
  IN     VOID                 *Context,
  IN     UINT8                SlotNum,
  IN OUT UINTN                *CertChainSize,
     OUT VOID                 *CertChain
  )
{
  return SpdmGetCertificateChooseLength(Context, SlotNum, MAX_SPDM_CERT_CHAIN_BLOCK_LEN, CertChainSize, CertChain);
}

/**
  This function sends GET_CERTIFICATE
  to get certificate chain in one slot from device.

  This function verify the integrity of the certificate chain.
  RootHash -> Root certificate -> Intermediate certificate -> Leaf certificate.

  If the peer root certificate hash is deployed,
  this function also verifies the digest with the root hash in the certificate chain.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SlotNum                      The number of slot for the certificate chain.
  @param  Length                       Length parameter in the get_certificate message (limited by MAX_SPDM_CERT_CHAIN_BLOCK_LEN).
  @param  CertChainSize                On input, indicate the size in bytes of the destination buffer to store the digest buffer.
                                       On output, indicate the size in bytes of the certificate chain.
  @param  CertChain                    A pointer to a destination buffer to store the certificate chain.

  @retval RETURN_SUCCESS               The certificate chain is got successfully.
  @retval RETURN_DEVICE_ERROR          A device error occurs when communicates with the device.
  @retval RETURN_SECURITY_VIOLATION    Any verification fails.
**/
RETURN_STATUS
EFIAPI
SpdmGetCertificateChooseLength (
  IN     VOID                 *Context,
  IN     UINT8                SlotNum,
  IN     UINT16               Length,
  IN OUT UINTN                *CertChainSize,
     OUT VOID                 *CertChain
  )
{
  SPDM_DEVICE_CONTEXT    *SpdmContext;
  UINTN                   Retry;
  RETURN_STATUS           Status;

  SpdmContext = Context;
  Retry = SpdmContext->RetryTimes;
  do {
    Status = TrySpdmGetCertificate(SpdmContext, SlotNum, Length, CertChainSize, CertChain);
    if (RETURN_NO_RESPONSE != Status) {
      return Status;
    }
  } while (Retry-- != 0);

  return Status;
}
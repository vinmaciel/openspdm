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
  UINT8                Reserved;
  UINT8                VersionNumberEntryCount;
  SPDM_VERSION_NUMBER  VersionNumberEntry[MAX_SPDM_VERSION_COUNT];
} SPDM_VERSION_RESPONSE_MAX;
#pragma pack()

/**
  This function sends GET_VERSION and receives VERSION.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  VersionCount                 VersionCount from the VERSION response.
  @param  VersionNumberEntries         VersionNumberEntries from the VERSION response.

  @retval RETURN_SUCCESS               The GET_VERSION is sent and the VERSION is received.
  @retval RETURN_DEVICE_ERROR          A device error occurs when communicates with the device.
**/
RETURN_STATUS
TrySpdmGetVersion (
  IN     SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN OUT UINT8                *VersionCount,
     OUT VOID                 *VersionNumberEntries
  )
{
  RETURN_STATUS                             Status;
  SPDM_GET_VERSION_REQUEST                  SpdmRequest;
  SPDM_VERSION_RESPONSE_MAX                 SpdmResponse;
  UINTN                                     SpdmResponseSize;
  UINTN                                     Index;
  UINT8                                     Version;
  UINTN                                     CompatibleVersionCount;
  SPDM_VERSION_NUMBER                       CompatibleVersionNumberEntry[MAX_SPDM_VERSION_COUNT];

  SpdmRequest.Header.SPDMVersion = SPDM_MESSAGE_VERSION_10;
  SpdmRequest.Header.RequestResponseCode = SPDM_GET_VERSION;
  SpdmRequest.Header.Param1 = 0;
  SpdmRequest.Header.Param2 = 0;
  Status = SpdmSendSpdmRequest (SpdmContext, NULL, sizeof(SpdmRequest), &SpdmRequest);
  if (RETURN_ERROR(Status)) {
    return RETURN_DEVICE_ERROR;
  }

  //
  // Cache data
  //
  ResetManagedBuffer (&SpdmContext->Transcript.MessageA);
  ResetManagedBuffer (&SpdmContext->Transcript.MessageB);
  ResetManagedBuffer (&SpdmContext->Transcript.MessageC);
  ResetManagedBuffer (&SpdmContext->Transcript.M1M2);
  AppendManagedBuffer (&SpdmContext->Transcript.MessageA, &SpdmRequest, sizeof(SpdmRequest));

  SpdmResponseSize = sizeof(SpdmResponse);
  ZeroMem (&SpdmResponse, sizeof(SpdmResponse));
  Status = SpdmReceiveSpdmResponse (SpdmContext, NULL, &SpdmResponseSize, &SpdmResponse);
  if (RETURN_ERROR(Status)) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponseSize < sizeof(SPDM_MESSAGE_HEADER)) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponse.Header.SPDMVersion != SPDM_MESSAGE_VERSION_10) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponse.Header.RequestResponseCode == SPDM_ERROR) {
    Status = SpdmHandleErrorResponseMain(SpdmContext, &SpdmContext->Transcript.MessageA, sizeof(SpdmRequest), &SpdmResponseSize, &SpdmResponse, SPDM_GET_VERSION, SPDM_VERSION, sizeof(SPDM_VERSION_RESPONSE_MAX));
    if (RETURN_ERROR(Status)) {
      return Status;
    }
  } else if (SpdmResponse.Header.RequestResponseCode != SPDM_VERSION) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponseSize < sizeof(SPDM_VERSION_RESPONSE)) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponseSize > sizeof(SpdmResponse)) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponse.VersionNumberEntryCount > MAX_SPDM_VERSION_COUNT) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponse.VersionNumberEntryCount == 0) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponseSize < sizeof(SPDM_VERSION_RESPONSE) + SpdmResponse.VersionNumberEntryCount * sizeof(SPDM_VERSION_NUMBER)) {
    return RETURN_DEVICE_ERROR;
  }
  SpdmResponseSize = sizeof(SPDM_VERSION_RESPONSE) + SpdmResponse.VersionNumberEntryCount * sizeof(SPDM_VERSION_NUMBER);
  //
  // Cache data
  //
  AppendManagedBuffer (&SpdmContext->Transcript.MessageA, &SpdmResponse, SpdmResponseSize);

  CompatibleVersionCount = 0;
  ZeroMem (&CompatibleVersionNumberEntry, sizeof(CompatibleVersionNumberEntry) * sizeof(SPDM_VERSION_NUMBER));
  for (Index = 0; Index < SpdmResponse.VersionNumberEntryCount; Index++) {
    Version = (UINT8)((SpdmResponse.VersionNumberEntry[Index].MajorVersion << 4) |
                                                         SpdmResponse.VersionNumberEntry[Index].MinorVersion);

    if(Version == SPDM_MESSAGE_VERSION_11 || Version == SPDM_MESSAGE_VERSION_10) {
         SpdmContext->ConnectionInfo.Version[Index] = Version;
         CompatibleVersionNumberEntry[CompatibleVersionCount] = SpdmResponse.VersionNumberEntry[Index];
         CompatibleVersionCount++;
    }
  }
  if(CompatibleVersionCount == 0) {
    return RETURN_DEVICE_ERROR;
  }

  if (VersionCount != NULL) {
    *VersionCount = CompatibleVersionCount;
    if (*VersionCount < CompatibleVersionCount) {
      return RETURN_BUFFER_TOO_SMALL;
    }

    if (VersionNumberEntries != NULL) {
      CopyMem (
        VersionNumberEntries,
        CompatibleVersionNumberEntry,
        CompatibleVersionCount * sizeof(SPDM_VERSION_NUMBER)
        );
    }
  }
  SpdmContext->SpdmCmdReceiveState |= SPDM_GET_VERSION_RECEIVE_FLAG;
  return RETURN_SUCCESS;
}

/**
  This function sends GET_VERSION and receives VERSION.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  VersionCount                 VersionCount from the VERSION response.
  @param  VersionNumberEntries         VersionNumberEntries from the VERSION response.

  @retval RETURN_SUCCESS               The GET_VERSION is sent and the VERSION is received.
  @retval RETURN_DEVICE_ERROR          A device error occurs when communicates with the device.
**/
RETURN_STATUS
EFIAPI
SpdmGetVersion (
  IN     SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN OUT UINT8                *VersionCount,
     OUT VOID                 *VersionNumberEntries
  )
{
  UINTN         Retry;
  RETURN_STATUS Status;

  Retry = SpdmContext->RetryTimes;
  do {
    Status = TrySpdmGetVersion(SpdmContext, VersionCount, VersionNumberEntries);
    if (RETURN_NO_RESPONSE != Status) {
      return Status;
    }
  } while (Retry-- != 0);

  return Status;
}

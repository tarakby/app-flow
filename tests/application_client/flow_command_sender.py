from enum import IntEnum, Enum
from typing import Generator, Optional
from contextlib import contextmanager
from bip_utils import Bip32Utils  # type: ignore[import]

from ragger.backend.interface import BackendInterface, RAPDU
from ragger.bip import CurveChoice
from ragger.utils.misc import split_message


MAX_APDU_LEN: int = 255
MAX_SLOTS = 64

class ClaType(IntEnum):
    """ Application ID definitions """
    # Application CLA
    CLA_APP = 0x33
    # Generic CLA
    CLA_GEN = 0xE0

class P1(IntEnum):
    """ Parameter 1 definitions """
    # Confirmation for GET_PUBKEY.
    P1_NO_CONFIRM = 0x00
    P1_CONFIRM = 0x01
    # Parameter 1 for SIGN.
    P1_INIT = 0x00
    P1_ADD  = 0x01
    P1_LAST = 0x02

class P2(IntEnum):
    """ Parameter 2 definitions """
    # Parameter 2 for SIGN P1_LAST
    P2_NO_METADATA = 0x01
    # Parameter 2 P2_MORE.
    P2_MORE = 0x80

class InsType(IntEnum):
    """ Application Command ID definitions """
    GET_VERSION     = 0x00
    GET_PUBKEY      = 0x01
    SIGN            = 0x02
    GET_SLOT_STATUS = 0x10
    GET_SLOT        = 0x11
    SET_SLOT        = 0x12
    GENERIC         = 0x01

class Errors(IntEnum):
    """ Application Errors definitions """
    SW_EXECUTION_ERROR          = 0x6400
    SW_WRONG_LENGTH             = 0x6700
    SW_EMPTY_BUFFER             = 0x6982
    SW_OUTPUT_BUFFER_TOO_SMALL  = 0x6983
    SW_DATA_INVALID             = 0x6984
    SW_CONDITIONS_NOT_SATISFIED = 0x6985
    SW_COMMAND_NOT_ALLOWED      = 0x6986
    SW_TX_NOT_INITIALIZED       = 0x6987
    SW_BAD_KEY_HANDLE           = 0x6A80
    SW_INVALIDP1P2              = 0x6B00
    SW_INS_NOT_SUPPORTED        = 0x6D00
    SW_CLA_NOT_SUPPORTED        = 0x6E00
    SW_UNKNOWN                  = 0x6F00
    SW_SIGN_VERIFY_ERROR        = 0x6F01
    SW_SUCCESS                  = 0x9000
    SW_BUSY                     = 0x9001

class HashType(str, Enum):
    """ Hash definitions """
    # SHA2-256
    HASH_SHA2 = "sha-2"
    # SHA3-256
    HASH_SHA3 = "sha-3"


def _pack_derivation(derivation_path: str) -> bytes:
    """ Pack derivation path in bytes """

    split = derivation_path.split("/")

    if split[0] != "m":
        raise ValueError("Error master expected")

    path_bytes: bytes = bytes()
    for value in split[1:]:
        if value == "":
            raise ValueError(f'Error missing value in split list "{split}"')
        if value.endswith('\''):
            path_bytes += Bip32Utils.HardenIndex(int(value[:-1])).to_bytes(4, byteorder='little')
        else:
            path_bytes += int(value).to_bytes(4, byteorder='little')

    return path_bytes


def _pack_crypto_option(curve: CurveChoice, hash_t: HashType) -> bytes:
    """ Pack crypto (curve + hash) options in bytes """

    path_bytes: bytes = bytes()

    if hash_t == HashType.HASH_SHA2:
        hash_value = 1
    elif hash_t == HashType.HASH_SHA3:
        hash_value = 3
    else:
        raise ValueError(f'Wrong Hash "{hash_t}"')
    path_bytes += int(hash_value).to_bytes(1, byteorder='little')

    if curve == CurveChoice.Nist256p1:
        curve_value = 2
    elif curve == CurveChoice.Secp256k1:
        curve_value = 3
    else:
        raise ValueError(f'Wrong Cruve "{curve}"')
    path_bytes += int(curve_value).to_bytes(1, byteorder='little')

    return path_bytes


def _format_apdu_data(
        curve: CurveChoice = CurveChoice.Secp256k1,
        hash_t: HashType = HashType.HASH_SHA2,
        path: str = "",
        address: str = "",
        slot: int = MAX_SLOTS,
) -> bytes:
    """ Format the data to be injected in the APDU """

    data_path: bytes = bytes()
    if slot != MAX_SLOTS:
        data_path += int(slot).to_bytes(1, byteorder='little')
    if address:
        data_path += bytes.fromhex(address)
    if path:
        data_path += _pack_derivation(path)
    if address == "0000000000000000" and path == "m/0/0/0/0/0":
        # Consider empty slot, force option to 0
        data_path += int(0).to_bytes(2, 'little')
    else:
        data_path += _pack_crypto_option(curve, hash_t)

    return data_path


class FlowCommandSender:
    """ Base class to send APDU to the selected backend """

    def __init__(self, backend: BackendInterface) -> None:
        self.backend = backend

    def get_generic(self) -> RAPDU:
        """ APDU generic version """

        data_path= _format_apdu_data(address="00")
        return self.backend.exchange(cla=ClaType.CLA_GEN,
                                    ins=InsType.GENERIC,
                                    data=data_path)

    def get_app_version(self) -> RAPDU:
        """ APDU app version """

        return self.backend.exchange(cla=ClaType.CLA_APP,
                                    ins=InsType.GET_VERSION)


    def get_slot_status(self) -> RAPDU:
        """ APDU slot status """

        return self.backend.exchange(cla=ClaType.CLA_APP,
                                    ins=InsType.GET_SLOT_STATUS)


    def get_slot(self, slot: int) -> RAPDU:
        """ APDU get slot """

        path_bytes: bytes = int(slot).to_bytes(1, byteorder='little')
        return self.backend.exchange(cla=ClaType.CLA_APP,
                                    ins=InsType.GET_SLOT,
                                    data=path_bytes)

    @contextmanager
    def set_slot(
        self,
        slot: int,
        address: str,
        path: str,
        curve: CurveChoice,
        hash_t: HashType,
    ) -> Generator[None, None, None]:
        """ APDU set slot """

        data_path= _format_apdu_data(curve, hash_t, path, address, slot)
        with self.backend.exchange_async(cla=ClaType.CLA_APP,
                                    ins=InsType.SET_SLOT,
                                    data=data_path) as response:
            yield response

    def get_public_key_no_confirmation(
            self,
            path: str,
            curve: CurveChoice,
            hash_t: HashType,
    ) -> RAPDU:
        """ APDU get public key - no confirmation """

        data_path= _format_apdu_data(curve, hash_t, path)
        return self.backend.exchange(cla=ClaType.CLA_APP,
                                    ins=InsType.GET_PUBKEY,
                                    p1=P1.P1_NO_CONFIRM,
                                    data=data_path)

    @contextmanager
    def get_public_key_with_confirmation(
        self,
        path: str,
        curve: CurveChoice,
        hash_t: HashType,
    ) -> Generator[None, None, None]:
        """ APDU get public key - with confirmation """

        data_path= _format_apdu_data(curve, hash_t, path)
        with self.backend.exchange_async(cla=ClaType.CLA_APP,
                                    ins=InsType.GET_PUBKEY,
                                    p1=P1.P1_CONFIRM,
                                    data=data_path) as response:
            yield response

    @contextmanager
    def sign_tx(
        self,
        path: str,
        curve: CurveChoice,
        transaction: bytes,
        hash_t: HashType,
    ) -> Generator[None, None, None]:
        """ APDU sign transaction """

        data_path= _format_apdu_data(curve, hash_t, path)
        self.backend.exchange(cla=ClaType.CLA_APP,
                              ins=InsType.SIGN,
                              p1=P1.P1_INIT,
                              data=data_path)

        messages = split_message(transaction, MAX_APDU_LEN)
        for msg in messages[:-1]:
            self.backend.exchange(cla=ClaType.CLA_APP,
                                  ins=InsType.SIGN,
                                  p1=P1.P1_ADD,
                                  data=msg)

        with self.backend.exchange_async(cla=ClaType.CLA_APP,
                                        ins=InsType.SIGN,
                                        p1=P1.P1_LAST,
                                        p2=P2.P2_NO_METADATA,
                                        data=messages[-1]) as response:
            yield response

    def get_async_response(self) -> Optional[RAPDU]:
        """ Asynchronous APDU response """

        return self.backend.last_async_response

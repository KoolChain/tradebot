from __future__ import print_function
import os.path
from googleapiclient.discovery import build
from google_auth_oauthlib.flow import InstalledAppFlow
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials

# If modifying these scopes, delete the file token.json.
# This is the complete read write scope
SCOPES = ['https://www.googleapis.com/auth/spreadsheets']


def initialize_interactive():
    """ The getting started autonomous initialization procedure
    """
    creds = None
    # The file token.json stores the user's access and refresh tokens, and is
    # created automatically when the authorization flow completes for the first
    # time.
    if os.path.exists('token.json'):
        creds = Credentials.from_authorized_user_file('token.json', SCOPES)
    # If there are no (valid) credentials available, let the user log in.
    if not creds or not creds.valid:
        if creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
        else:
            flow = InstalledAppFlow.from_client_secrets_file(
                'credentials.json', SCOPES)
            creds = flow.run_local_server(port=0)
        # Save the credentials for the next run
        with open('token.json', 'w') as token:
            token.write(creds.to_json())

    service = build('sheets', 'v4', credentials=creds)
    return service


def authorize(credentials_file, output_file):
    flow = InstalledAppFlow.from_client_secrets_file(credentials_file, SCOPES)
    creds = flow.run_local_server(port=0)
    # Save the credentials
    with open(output_file, 'w') as token:
        token.write(creds.to_json())


class Spreadsheet(object):

    def __init__(self, token_file, spreadsheet_id):
        if os.path.exists(token_file):
            creds = Credentials.from_authorized_user_file(token_file, SCOPES)
            if not creds.valid:
                if creds and creds.expired and creds.refresh_token:
                    creds.refresh(Request())
                    with open(token_file, 'w') as token:
                        token.write(creds.to_json())
                else:
                    raise Exception("Unable to obtain valid credentials.")
            self.service = build('sheets', 'v4', credentials=creds)
        else:
            raise Exception("Cannot find provided token file: {}.".format(token_file))

        self.spreadsheet_id = spreadsheet_id


    def read(self, range_a1):
        sheet = self.service.spreadsheets()
        result = sheet.values().get(spreadsheetId=self.spreadsheet_id,
                                    range=range_a1).execute()
        return result.get('values', [])


    def append(self, range_a1, values):
        """ range_a1: can be a sheet name """
        """ return the number of cells appended """
        body = {
            'values': values
        }

        result = self.service.spreadsheets().values().append(
            spreadsheetId=self.spreadsheet_id, range=range_a1,
            valueInputOption="USER_ENTERED", body=body).execute()

        return result.get('updates').get('updatedCells')

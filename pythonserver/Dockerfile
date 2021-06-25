# syntax=docker/dockerfile:1

FROM python:3.9-alpine
COPY requirements.txt ./
RUN pip3 install -r requirements.txt
COPY dbaccess.py gsheet.py updatesheet.py ./

COPY docker/updatesheet.sh /etc/periodic/15min/updatesheet
CMD ["crond","-f", "-l", "6"]

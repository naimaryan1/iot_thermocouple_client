#author: ary naim ECE531
#1) scp this script to the cloud
#2) make sure you have Python3 & Flask installed
#run in the backgroud: nohup python3 IoTWebServiceFinal.py > IoT_WS_Final.txt &
#check logs using "tail -10 IoT_WS_Final.txt"
''''
Example calls:

curl -H "Content-Type: application/json" -X POST -d '{"id": "1000","state": "1","temp": "0.00","set_point_1_time_hh_mm": "HH:MM","set_point_2_time_hh_mm": "HH:MM","set_point_3_time_hh_mm": "HH:MM","set_point_1": "0.00","set_point_2": "0.00","set_point_3": "0.00"}' "http://20.163.108.241:5000/create_iot_device"

curl -H "Content-Type: application/json" -X POST -d '{"id": "1000","state": "1","temp": "0.00","set_point_1_time_hh_mm": "HH:MM","set_point_2_time_hh_mm": "HH:MM","set_point_3_time_hh_mm": "HH:MM","set_point_1": "0.00","set_point_2": "0.00","set_point_3": "0.00"}' "http://20.163.108.241:5000/update_iot_device"

curl -H "Content-Type: application/json" -X GET -d '{"id": "1000"}' "http://20.163.108.241:5000/find_iot_device"

curl -H "Content-Type: application/json" -X GET -d '{"id": "1000"}' "http://20.163.108.241:5000/delete_iot_device"

'''
from flask import Flask, request, jsonify

app = Flask(__name__)

# Global dictionary to store IoT objects
iot_objects = {}


@app.route('/create_iot_device', methods=['POST'])
def create_iot_device():
    try:
        data = request.get_json()
        if not data:
            return jsonify({"message": "error"}), 400

        id_val = data.get('id')
        if not id_val:
            return jsonify({"message": "error"}), 400

        if id_val in iot_objects:
            return jsonify({"message": "This already exists"}), 200

        iot_objects[id_val] = data
        return jsonify({"message": "Success"}), 200
    except Exception as e:
        return jsonify({"message": str(e)}), 500


@app.route('/find_iot_device', methods=['GET'])
def find_iot_device():
    try:
        data = request.get_json()
        if not data:
            return jsonify({"message": "error"}), 400

        id_val = data.get('id')
        if not id_val:
            return jsonify({"message": "error"}), 400

        if id_val not in iot_objects:
            return jsonify({"message": "Unsuccessful IoT device is not present"}), 404
        else:
            return jsonify(iot_objects[id_val]), 200

    except Exception as e:
        return jsonify({"message": str(e.msg)}), 500


@app.route('/update_iot_device', methods=['POST', 'PUT'])
def update_iot_device():
    try:
        data = request.get_json()
        if not data:
            return jsonify({"message": "error"}), 400

        id_val = data.get('id')
        if not id_val:
            return jsonify({"message": "error"}), 400

        if id_val not in iot_objects:
            return jsonify({"message": "Unsuccessful IoT device is not present"}), 404

        iot_objects[id_val] = data
        return jsonify({"message": "successful"}), 200
    except Exception as e:
        return jsonify({"message": str(e)}), 500


@app.route('/delete_iot_device', methods=['GET'])
def delete_iot_device():
    try:
        data = request.get_json()
        if not data:
            return jsonify({"message": "error"}), 400

        id_val = data.get('id')
        if not id_val:
            return jsonify({"message": "error"}), 400

        if id_val not in iot_objects:
            return jsonify({"message": "Unsuccessful IoT device is not present"}), 404
        else:
            del iot_objects[id_val]
            return jsonify({"message": "Device successfully deleted"}), 200
    except Exception as e:
        return jsonify({"message": str(e)}), 500


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)


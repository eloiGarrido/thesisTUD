import matplotlib.pyplot as plt
import sys
import math
import numpy as np

class PID:
    """
    Discrete PID control
    """

    

    def __init__(self, P=2.0, I=0.0, D=1.0, Derivator=0, Integrator=0, Integrator_max=500, Integrator_min=-500):

        self.Kp = P
        self.Ki = I
        self.Kd = D
        # self.Derivator_array = [0,0,0,0,0]
        self.Derivator_array = 0
        self.Derivator = Derivator
        # self.Derivator = Derivator

        self.Integrator = Integrator
        self.Integrator_max = Integrator_max
        self.Integrator_min = Integrator_min

        self.set_point = 0.0
        self.error = 0.0

        self.derivator_counter = 0
    def update(self, current_value, hr):
        """
        Calculate PID output value for given reference input and feedback
        """

        self.error = self.set_point - current_value

        self.P_value = self.Kp * self.error
        self.D_value = self.Kd * (self.error - self.Derivator)
        self.Derivator = self.error

        # self.D_value = self.Kd * (hr - sum(self.Derivator_array)/5 )
        self.D_value = self.Kd * (hr - self.Derivator_array )
        # self.Derivator_array[self.derivator_counter % 5] = hr
        self.Derivator_array = hr
        self.derivator_counter = self.derivator_counter + 1

        self.Integrator = self.Integrator + self.error

        if self.Integrator > self.Integrator_max:
            self.Integrator = self.Integrator_max
        elif self.Integrator < self.Integrator_min:
            self.Integrator = self.Integrator_min

        self.I_value = self.Integrator * self.Ki

        PID = self.P_value + self.I_value + self.D_value

        return PID

    def setPoint(self, set_point):
        """
        Initilize the setpoint of PID
        """
        self.set_point = set_point
        self.Integrator = 0
        self.Derivator = 0

    def setIntegrator(self, Integrator):
        self.Integrator = Integrator

    def setDerivator(self, Derivator):
        self.Derivator = Derivator

    def setKp(self, P):
        self.Kp = P

    def setKi(self, I):
        self.Ki = I

    def setKd(self, D):
        self.Kd = D

    def getPoint(self):
        return self.set_point

    def getError(self):
        return self.error

    def getIntegrator(self):
        return self.Integrator

    def getDerivator(self):
        return self.Derivator


# class DcPidController:
#     def __init__(self, kp, ki, kd, tf, set_point):
#         self.PIDController = PID(kp,ki,kd,tf)
#         self.PIDController.setPoint(set_point)
def read_energy_file():
    with open('energy_values.txt') as f:
        lines = f.readlines()
    return lines


def print_data(data, name):
    plt.figure()
    plt.plot(data)
    plt.title(name)
    plt.draw()
    plt.savefig('PID/' + name + '.eps', format='eps')




def pid_controll():
    PIDController = PID(kp, ki, kd, tf)
    PIDController.setPoint(E_SP)
    consumption = 0
    increment = 10

    DC_array = []
    Energy_array = []
    Harvest_array = []
    Consumption_array = []

    for line in E:
        if line == 'nan\n':
            print ('BREAK')
            break
        harvest = float(line)
        for i in range (0,30):
            if E_t < (DC_init * E_per_ms):
                E_t = E_t + harvest
                result = PIDController.update(E_t, harvest)
                continue

            consumption = DC * E_per_ms
            E_t = E_t + harvest - consumption
            # E_t = E_t + harvest - consumption
            E_t = min (E_t, E_MAX)
            if E_t < 0:
                E_t = 0
            result = PIDController.update(E_t, harvest*100)
            try:
                increment = int(DC_init + (-1 * result)/SCALATE_OUTPUT)
            except:
                print('Error')
            DC = max(DC_init, increment )
            if DC > DC_max:
                DC = DC_max
            # DC = min(200, DC)

        DC_array.append(DC)
        Energy_array.append(E_t)
        Harvest_array.append(harvest)
        Consumption_array.append(consumption)

    return DC_array, Energy_array, Harvest_array, Consumption_array

def square_error_control(E):
    DC_array = []
    Energy_array = []
    Harvest_array = []
    Consumption_array = []
    # Initial values
    DC_init = 10.0  # 10 ms
    DC = DC_init

    theta = np.array([2,-1,1])
    B = 0.95
    B_goal = 0.85
    # Energy in mF
    E_0 = 22500.0 #mJ
    E_t = E_0
    E_per_ms = 0.075

    dc_min = 0.01
    dc_max = 0.20

    DC_0 = 0.01
    rho = DC_0 #Initial DC
    u = DC_0
    u_avg = DC_0
    phi = np.array([B, u, -B_goal])
    consumption = 0
    mu = 0.001

    Alpha = 0.65
    Beta = 0.3

    for line in E:
        if line == 'nan\n':
            harvest = 0
        else:
            harvest = float(line)


        for i in range (0,30): # Same value for the next 30 seconds
            if E_t < (DC_init * E_per_ms):
                E_t = E_t + harvest
                consumption = 0
                DC = 0
                DC_array.append(DC)
                Energy_array.append(E_t)
                Harvest_array.append(harvest)
                Consumption_array.append(consumption)
                continue

            consumption = DC * E_per_ms
            E_t = E_t + harvest - consumption
            E_t = min (E_t, E_MAX)
            E_t = max (0, E_t)


            B = E_t / E_0

            theta = theta + ((mu / (sum(phi * phi)))* phi * (B - sum(phi * theta)))

            u = (B_goal - theta[0]*B + theta[2]*B_goal) / theta[1]

            u = min(dc_max, u)
            u = max(dc_min, u)

            phi = np.array([B, u, -B_goal])

            u_avg = Alpha*u + (1-Alpha)*u_avg

            rho = Beta*u + (1-Beta)*u_avg

            DC = int(rho * 1000)

        DC_array.append(DC)
        Energy_array.append(E_t)
        Harvest_array.append(harvest)
        Consumption_array.append(consumption)

    return DC_array, Energy_array, Harvest_array, Consumption_array



if __name__ == '__main__':

    kp = 1
    ki = 0
    kd = 500
    tf = 1
    E_SP = 13500.0  # mF
    E_MAX = 22500.0
    DC_init = 10.0  # 10 ms
    DC_max = 200.0
    SCALATE_OUTPUT = 3000
    period = 1  # seconds
    E_per_ms = 0.075
    DC = DC_init
    E_t = 22500.0
    B = 1
    B_prima = 0.75

    E = read_energy_file()

    controller = 'SQUARE'
    if controller == 'SQUARE':
        DC_array, Energy_array, Harvest_array, Consumption_array = square_error_control(E)
    else:
        DC_array, Energy_array, Harvest_array, Consumption_array = pid_controll()


    print_data(DC_array, 'DC')
    print_data(Energy_array, 'Energy')
    print_data(Harvest_array, 'Harv')
    print_data(Consumption_array, 'Cons')
    plt.show()

